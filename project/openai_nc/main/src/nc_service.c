#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "linked_list.h"
#include "my_log.h"
#include "openai_api.h"

#define TAG "NC_SERVICE"

#define MAX_LISTEN 20
#define MAX_FD_NUM 20

static FD_INFO *FdInfoCreate(int fd) {
    FD_INFO *fdInfo = (FD_INFO *)malloc(sizeof(FD_INFO) + BUF_SIZE);
    if (NULL == fdInfo) {
        return NULL;
    }
    bzero(fdInfo, sizeof(FD_INFO) + BUF_SIZE);
    fdInfo->fd = fd;
    fdInfo->buf_cap = BUF_SIZE;
    return fdInfo;
}

static void FdInfoReset(FD_INFO *fdInfo) {
    assert(NULL != fdInfo && 0 != fdInfo->buf_cap);
    memset(fdInfo->buffer, 0, fdInfo->buf_cap);
    fdInfo->buf_len = 0;
}

static void FdInfoDestory(FD_INFO **fdInfo) {
    assert(NULL != *fdInfo);
    free(*fdInfo);
    *fdInfo = NULL;
}

// 设置非阻塞句柄
static void SetNonBlock(int fd) {
    int flag = fcntl(fd, F_GETFL, 0);
    if (flag == -1) {
        MY_LOGW(TAG, "get fcntl flag: %s", strerror(errno));
        return;
    }
    int ret = fcntl(fd, F_SETFL, flag | O_NONBLOCK);
    if (ret == -1) {
        MY_LOGW(TAG, "set fcntl non-blocking: %s", strerror(errno));
        return;
    }
}

// 创建非阻塞套接字，绑定 ip 与 port
int SocketCreate(const char *ip, uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == -1) {
        MY_LOGE(TAG, "socket create: %s", strerror(errno));
        return -1;
    }
    SetNonBlock(fd);
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons(port);
    memset(addr.sin_zero, 0, sizeof(addr.sin_zero));

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        MY_LOGE(TAG, "socket bind: %s", strerror(errno));
        return -1;
    }
    if (listen(fd, MAX_LISTEN) == -1) {
        MY_LOGE(TAG, "socket listen: %s", strerror(errno));
        return -1;
    }
    return fd;
}

static int8_t equal(void *A, void *B) {
    return (*(FD_INFO **)A)->fd == (*(FD_INFO **)B)->fd;
}

static void *callback(LinkedListNode *p) {
    FdInfoDestory(&(p->data));
    return NULL;
}

typedef enum {
    M_CTRL,
    M_API,
    M_NOTICE,
} MODE;

// 异步监听套接字
void SocketAccept(int fd) {
    LinkedList client_fds;
    LinkedListInit(&client_fds);

    struct epoll_event event, e_stdin, events[MAX_FD_NUM];
    int client_fd;
    int epfd = epoll_create(MAX_FD_NUM);
    if (epfd == -1) {
        MY_LOGE(TAG, "epoll create %s", strerror(errno));
        goto linklist_clean;
    }

    memset(&event, 0, sizeof(event));
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event) == -1) {
        MY_LOGE(TAG, "epoll ctl %s", strerror(errno));
        goto linklist_clean;
    }

    SetNonBlock(STDIN_FILENO);
    memset(&e_stdin, 0, sizeof(e_stdin));
    e_stdin.data.fd = STDIN_FILENO;
    e_stdin.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &e_stdin) == -1) {
        MY_LOGE(TAG, "epoll ctl %s\n", strerror(errno));
        goto linklist_clean;
    }

    MODE mode = M_CTRL;
    FD_INFO *fd_new, *fd_tmp;
    FD_INFO *fd0 = FdInfoCreate(STDIN_FILENO);
    OPENAI_PARAM openai_param = {
        .max_tokens = 4000,
        .temperature = 0.8f,
        .top_p = 1.0f,
        .frequency_penalty = 0.0f,
        .presence_penalty = 0.0f,
    };
    int ret = 0;
    while (1) {
        int num = epoll_wait(epfd, events, MAX_FD_NUM, -1);
        if (num == -1) {
            MY_LOGE(TAG, "epoll wait %s", strerror(errno));
            break;
        } else {
            // 遍历处理 epoll 接收到的事件
            for (int i = 0; i < num; ++i) {
                if (events[i].data.fd == STDIN_FILENO) {
                    /* 标准输入事件 */
                    FdInfoReset(fd0);
                    while (fd0->buf_len < fd0->buf_cap) {
                        ret = read(0, fd0->buffer + fd0->buf_len, fd0->buf_cap - fd0->buf_len);
                        if (ret < 0) {
                            if (errno == EAGAIN) {
                                MY_LOGCI(TAG, "stdin: %s", fd0->buffer);
                                break;
                            }
                            MY_LOGW(TAG, "stdin error: %s", strerror(errno));
                            break;
                        }
                        fd0->buf_len += ret;
                    }
                    if (!strncmp(fd0->buffer, "mode ctrl", 9)) {
                        mode = M_CTRL;
                        MY_LOGI(TAG, "mode translate to ctrl");
                        continue;
                    } else if (!strncmp(fd0->buffer, "mode api", 8)) {
                        mode = M_API;
                        MY_LOGI(TAG, "mode translate to api");
                        continue;
                    } else if (!strncmp(fd0->buffer, "mode notice", 11)) {
                        mode = M_NOTICE;
                        MY_LOGI(TAG, "mode translate to notice");
                        continue;
                    } else if (!strncmp(fd0->buffer, "mode", 4)) {
                        switch (mode) {
                            case M_CTRL: {
                                MY_LOGI(TAG, "mode: ctrl");
                            } break;
                            case M_API: {
                                MY_LOGI(TAG, "mode: api");
                            } break;
                            case M_NOTICE: {
                                MY_LOGI(TAG, "mode: notice");
                            } break;
                            default: {
                                MY_LOGW(TAG, "Wrong pattern");
                            } break;
                        }
                        continue;
                    }
                    switch (mode) {
                        case M_CTRL: {
                            if (!strncmp(fd0->buffer, "exit", 4)) {
                                MY_LOGI(TAG, "End of service");
                                return;
                            } else if (!strncmp(fd0->buffer, "set ", 4)) {
                                sscanf(fd0->buffer + 4, "%d%f%f%f%f",
                                       &openai_param.max_tokens,
                                       &openai_param.temperature,
                                       &openai_param.top_p,
                                       &openai_param.frequency_penalty,
                                       &openai_param.presence_penalty);
                                MY_LOGI(TAG, "       max_tokens: %d", openai_param.max_tokens);
                                MY_LOGI(TAG, "      temperature: %.1f", openai_param.temperature);
                                MY_LOGI(TAG, "            top_p: %.1f", openai_param.top_p);
                                MY_LOGI(TAG, "frequency_penalty: %.1f", openai_param.frequency_penalty);
                                MY_LOGI(TAG, " presence_penalty: %.1f", openai_param.presence_penalty);
                            } else if (!strncmp(fd0->buffer, "list", 4)) {
                                MY_LOGCI(TAG, "client_fds: [ ");
                                for (LinkedListNode *p = client_fds.root; p != NULL; p = p->next) {
                                    printf("%d ", p->data->fd);
                                }
                                printf("]\n");
                            }
                        } break;
                        case M_API: {
                            // TODO: 调用 openai api 问问题，由于同一个 key，所以这里阻塞，各个客户端请求排队处理，不能并行
                            cJSON *resp = openai_api_run(fd0->buffer, &openai_param);
                            if (NULL == resp) {
                                MY_LOGW(TAG, "openai return error: %d", __LINE__);
                                continue;
                            }
                            cJSON *chs = cJSON_GetObjectItem(resp, "choices");
                            if (NULL == chs) {
                                MY_LOGW(TAG, "openai return error: %d", __LINE__);
                                cJSON_Delete(resp);
                                continue;
                            }
                            // 有 choices 下面就不会出错
                            cJSON *itm = cJSON_GetArrayItem(chs, 0);
                            char *txt = cJSON_GetObjectItem(itm, "text")->valuestring;
                            MY_LOGCI(TAG, "api: %s\n", txt);
                            cJSON_Delete(resp);
                        } break;
                        case M_NOTICE: {
                            // 处理每一个 client
                            for (LinkedListNode *p = client_fds.root; p != NULL; p = p->next) {
                                ret = send(p->data->fd, "\rnotice: ", 9, 0);
                                ret = send(p->data->fd, fd0->buffer, fd0->buf_len, 0);
                                ret = send(p->data->fd, "> ", 2, 0);
                                if (ret < 0) {
                                    MY_LOGW(TAG, "send error: %s", strerror(errno));
                                    // 中途有客户端断连或其他错误 跳过
                                    continue;
                                }
                                MY_LOGCI(TAG, "[%d] send: %s", p->data->fd, fd0->buffer);
                            }
                        } break;
                        default: {
                            MY_LOGW(TAG, "Wrong pattern");
                        } break;
                    }
                } else if (events[i].data.fd == fd) {
                    /* 监听端套接字事件 */
                    struct sockaddr_in client_addr;
                    memset(&client_addr, 0, sizeof(client_addr));
                    socklen_t len = sizeof(client_addr);
                    client_fd = accept(fd, (struct sockaddr *)&client_addr, &len);
                    if (client_fd == -1) {
                        MY_LOGE(TAG, "socket accept %s", strerror(errno));
                        goto linklist_clean;
                    } else {
                        MY_LOGCI(TAG, "socket accept success. fd=%d, client_fd=%d\n", fd, client_fd);
                        fd_new = FdInfoCreate(client_fd);
                        if (NULL == fd_new) {
                            MY_LOGE(TAG, "FdInfoCreate error %s", strerror(errno));
                            goto linklist_clean;
                        }
                        ret = LinkedListAppend(&client_fds, fd_new);
                        if (!ret) {
                            MY_LOGE(TAG, "LinkList append %s", strerror(errno));
                            goto linklist_clean;
                        }
                        ret = send(client_fd, "> ", 2, 0);
                        if (ret < 0) {
                            MY_LOGW(TAG, "send error: %s", strerror(errno));
                            continue;
                        }
                    }
                    SetNonBlock(client_fd);
                    event.data.fd = client_fd;
                    event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET;
                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                        MY_LOGE(TAG, "epoll ctl %s", strerror(errno));
                        goto linklist_clean;
                    }
                } else if (events[i].events & EPOLLERR || events[i].events & EPOLLHUP) {
                    /* epoll 内核错误事件 */
                    MY_LOGE(TAG, "epoll err");
                    close(events[i].data.fd);
                } else {
                    /* 客户端套接字事件 */
                    int index = 0;
                    index = LinkedListLocate(&client_fds, &(FD_INFO){.fd = events[i].data.fd}, equal);
                    if (index < 0) {
                        MY_LOGE(TAG, "fd not found");
                        goto linklist_clean;
                    }
                    LinkedListGet(&client_fds, index, &fd_tmp);
                    FdInfoReset(fd_tmp);
                    while (fd_tmp->buf_len < fd_tmp->buf_cap) {
                        // int recv(描述符，空间，数据长度，标志位)
                        // 返回值：实际获取大小， 0-连接断开； -1-出错了
                        ret = recv(events[i].data.fd, fd_tmp->buffer + fd_tmp->buf_len, fd_tmp->buf_cap - fd_tmp->buf_len, 0);
                        if (ret < 0) {
                            if (errno == EAGAIN) {
                                // 所有数据都读完了
                                MY_LOGCI(TAG, "[%d] recv msg: %s\n", events[i].data.fd, fd_tmp->buffer);
                                if (fd_tmp->buf_len <= 3) {
                                    ret = send(events[i].data.fd, "None\n\n> ", 8, 0);
                                    if (ret < 0) {
                                        MY_LOGW(TAG, "send error: %s", strerror(errno));
                                        break;
                                    }
                                    MY_LOGI(TAG, "[%d] send: None", events[i].data.fd);
                                    break;
                                } else if (!strncmp(fd_tmp->buffer, "exit", 4)) {
                                    MY_LOGI(TAG, "peer exit: %d", events[i].data.fd);
                                    goto remove_client;
                                }
                                // TODO: 调用 openai api 问问题，由于同一个 key，所以这里阻塞，各个客户端请求排队处理，不能并行
                                cJSON *response = openai_api_run(fd_tmp->buffer, &openai_param);
                                if (NULL == response) {
                                    MY_LOGW(TAG, "openai return error: %d", __LINE__);
                                    ret = send(events[i].data.fd, "\n\n> ", 4, 0);
                                    break;
                                }
                                cJSON *choices = cJSON_GetObjectItem(response, "choices");
                                if (NULL == choices) {
                                    MY_LOGW(TAG, "openai return error: %d", __LINE__);
                                    ret = send(events[i].data.fd, "\n\n> ", 4, 0);
                                    cJSON_Delete(response);
                                    break;
                                }
                                // 有 choices 下面就不会出错
                                cJSON *item = cJSON_GetArrayItem(choices, 0);
                                char *text = cJSON_GetObjectItem(item, "text")->valuestring;
                                ret = send(events[i].data.fd, text, strlen(text), 0);
                                if (ret < 0) {
                                    MY_LOGW(TAG, "send error: %s", strerror(errno));
                                    break;
                                }
                                MY_LOGCI(TAG, "[%d] send: %s\n", events[i].data.fd, text);
                                cJSON_Delete(response);
                                ret = send(events[i].data.fd, "\n\n> ", 4, 0);
                                if (ret < 0) {
                                    MY_LOGW(TAG, "send error: %s", strerror(errno));
                                    break;
                                }
                                break;
                            }
                            MY_LOGW(TAG, "recv error: %s", strerror(errno));
                            goto remove_client;
                        } else if (ret == 0) {
                            MY_LOGI(TAG, "peer shutdown: %d", events[i].data.fd);
                        remove_client:
                            close(events[i].data.fd);
                            ret = LinkedListRemove(&client_fds, fd_tmp, equal, callback);
                            if (ret < 0) {
                                MY_LOGE(TAG, "LinkedListRemove failed %s", strerror(errno));
                            }
                            break;
                        } else {
                            fd_tmp->buf_len += ret;
                        }
                    }
                }
            }
        }
    }
linklist_clean:
    FdInfoDestory(&fd0);
    LinkedListClean(&client_fds, callback);
}

// int EpollSocket(void) {
//     openai_api_init();

//     int fd = SocketCreate("0.0.0.0", 8888);
//     MY_LOGCI(TAG, "EpollSocket run at 0.0.0.0:8888\n");
//     if (fd == -1) {
//         MY_LOGCI(TAG, "socket create fd failed\n");
//         return -1;
//     }
//     SocketAccept(fd);

//     openai_api_deinit();
//     return 0;
// }
