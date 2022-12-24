#include <arpa/inet.h>
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

#define NC_SERVICE_BUF_SIZE 4096

static int FdInfoInit(FdInfo_s * fdInfo, int fd) {
    char* ret = malloc(NC_SERVICE_BUF_SIZE);
    if (ret == NULL) {
        MY_LOGCE(TAG, "FdInfoCreate malloc error: %s\n", strerror(errno));
        return -1;
    }
    fdInfo->fd = fd;
    fdInfo->pBuff = ret;
    fdInfo->total = 0;
    memset(fdInfo->pBuff, 0, NC_SERVICE_BUF_SIZE);
    return 0;
}

static void FdInfoReset(FdInfo_s * fdInfo) {
    memset(fdInfo->pBuff, 0, NC_SERVICE_BUF_SIZE);
    fdInfo->total = 0;
}

static void FdInfoDeinit(FdInfo_s * fdInfo) {
    free(fdInfo->pBuff);
    fdInfo->fd = 0;
    fdInfo->pBuff = NULL;
    fdInfo->total = 0;
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
    return ((FdInfo_s *)A)->fd == ((FdInfo_s *)B)->fd;
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
    char buf[NC_SERVICE_BUF_SIZE] = {0};
    int buf_size = sizeof(buf);
    int buf_len = 0;
    int max_tokens = 4000;
    float temperature = 0.8;
    float top_p = 1.0;
    float frequency_penalty = 0.0;
    float presence_penalty = 0.0;
    int ret = 0;
    FdInfo_s fdInfoTmp;
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
                    memset(buf, 0, sizeof(buf));
                    buf_len = 0;
                    while (buf_len < buf_size) {
                        ret = read(0, buf + buf_len, buf_size - buf_len);
                        if (ret < 0) {
                            if (errno == EAGAIN) {
                                MY_LOGCI(TAG, "stdin: %s", buf);
                                break;
                            }
                            MY_LOGW(TAG, "stdin error: %s", strerror(errno));
                            break;
                        }
                        buf_len += ret;
                    }
                    if (!strncmp(buf, "mode ctrl", 9)) {
                        mode = M_CTRL;
                        MY_LOGI(TAG, "mode translate to ctrl");
                        continue;
                    } else if (!strncmp(buf, "mode api", 8)) {
                        mode = M_API;
                        MY_LOGI(TAG, "mode translate to api");
                        continue;
                    } else if (!strncmp(buf, "mode notice", 11)) {
                        mode = M_NOTICE;
                        MY_LOGI(TAG, "mode translate to notice");
                        continue;
                    } else if (!strncmp(buf, "mode", 4)) {
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
                            if (!strncmp(buf, "exit", 4)) {
                                MY_LOGI(TAG, "End of service");
                                return;
                            } else if (!strncmp(buf, "set ", 4)) {
                                sscanf(buf + 4, "%d%f%f%f%f", &max_tokens, &temperature, &top_p, &frequency_penalty, &presence_penalty);
                                MY_LOGI(TAG, "       max_tokens: %d", max_tokens);
                                MY_LOGI(TAG, "      temperature: %.1f", temperature);
                                MY_LOGI(TAG, "            top_p: %.1f", top_p);
                                MY_LOGI(TAG, "frequency_penalty: %.1f", frequency_penalty);
                                MY_LOGI(TAG, " presence_penalty: %.1f", presence_penalty);
                            } else if (!strncmp(buf, "list", 4)) {
                                MY_LOGCI(TAG, "client_fds: [ ");
                                for (LinkedListNode *p = client_fds.root; p != NULL; p = p->next) {
                                    printf("%d ", p->data.fd);
                                }
                                printf("]\n");
                            }
                        } break;
                        case M_API: {
                            // TODO: 调用 openai api 问问题，由于同一个 key，所以这里阻塞，各个客户端请求排队处理，不能并行
                            cJSON *resp = openai_api_run(buf, max_tokens, temperature, top_p, frequency_penalty, presence_penalty);
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
                                ret = send(p->data.fd, "\rnotice: ", 9, 0);
                                ret = send(p->data.fd, buf, buf_len, 0);
                                ret = send(p->data.fd, "> ", 2, 0);
                                if (ret < 0) {
                                    MY_LOGW(TAG, "send error: %s", strerror(errno));
                                    // 中途有客户端断连或其他错误 跳过
                                    continue;
                                }
                                MY_LOGCI(TAG, "[%d] send: %s", p->data.fd, buf);
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
                        ret = FdInfoInit(&fdInfoTmp, client_fd);
                        if (ret < 0) {
                            goto linklist_clean;
                        }
                        ret = LinkedListAppend(&client_fds, fdInfoTmp);
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
                    fdInfoTmp.fd = events[i].data.fd;
                    index = LinkedListLocate(&client_fds, fdInfoTmp, equal);
                    LinkedListGet(&client_fds, index, &fdInfoTmp);
                    while (fdInfoTmp.total < buf_size) {
                        // int recv(描述符，空间，数据长度，标志位)
                        // 返回值：实际获取大小， 0-连接断开； -1-出错了
                        ret = recv(events[i].data.fd, fdInfoTmp.pBuff + fdInfoTmp.total, buf_size - fdInfoTmp.total, 0);
                        if (ret < 0) {
                            if (errno == EAGAIN) {
                                if(fdInfoTmp.pBuff == NULL ||
                                    *(char*)(fdInfoTmp.pBuff + fdInfoTmp.total- 1) == '\n') {
                                    // 最后一个字符是\n，数据没读完，继续接收
                                    break;
                                }
                                // 所有数据都读完了
                                MY_LOGCI(TAG, "[%d] recv msg: %s\n", events[i].data.fd, fdInfoTmp.pBuff);
                                if (fdInfoTmp.total <= 3) {
                                    ret = send(events[i].data.fd, "None\n\n> ", 8, 0);
                                    if (ret < 0) {
                                        MY_LOGW(TAG, "send error: %s", strerror(errno));
                                        break;
                                    }
                                    MY_LOGI(TAG, "[%d] send: None", events[i].data.fd);
                                    break;
                                } else if (!strncmp(buf, "exit", 4)) {
                                    MY_LOGI(TAG, "peer exit: %d", events[i].data.fd);
                                    goto remove_client;
                                }
                                // TODO: 调用 openai api 问问题，由于同一个 key，所以这里阻塞，各个客户端请求排队处理，不能并行
                                cJSON *response = openai_api_run(fdInfoTmp.pBuff, max_tokens, temperature, top_p, frequency_penalty, presence_penalty);
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
                                FdInfoReset(&fdInfoTmp);
                                break;
                            }
                            MY_LOGW(TAG, "recv error: %s", strerror(errno));
                            goto remove_client;
                        } else if (ret == 0) {
                            MY_LOGI(TAG, "peer shutdown: %d", events[i].data.fd);
                        remove_client:
                            close(events[i].data.fd);
                            LinkedListRemove(&client_fds, fdInfoTmp, equal);
                            FdInfoDeinit(&fdInfoTmp);
                            break;
                        } else /*if(ret > 0)*/ {
                            fdInfoTmp.total += ret;
                        }
                    }
                    if(fdInfoTmp.pBuff != NULL) {
                        // 还存在链表中，更新数据
                        LinkedListModify(&client_fds, index, fdInfoTmp);
                    }
                }
            }
        }
    }
linklist_clean:
    LinkedListClean(&client_fds);
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
