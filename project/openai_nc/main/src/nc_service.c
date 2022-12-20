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

#include "my_log.h"
#define LINKED_LIST_TYPE int
#include "linked_list.h"
#include "openai_api.h"

#define TAG "NC_SERVICE"

#define MAX_LISTEN 20
#define MAX_FD_NUM 20

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
    return *(int *)A == *(int *)B;
}

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

    SetNonBlock(0);
    memset(&e_stdin, 0, sizeof(e_stdin));
    e_stdin.data.fd = 0;
    e_stdin.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, 0, &e_stdin) == -1) {
        MY_LOGE(TAG, "epoll ctl %s", strerror(errno));
        goto linklist_clean;
    }

    char buf[4096] = {0};
    int buf_size = sizeof(buf);
    int total = 0;
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
                    memset(buf, 0, sizeof(buf));
                    total = 0;
                    while (total < buf_size) {
                        ret = read(0, buf + total, buf_size - total);
                        if (ret < 0) {
                            if (errno == EAGAIN) {
                                MY_LOGCI(TAG, "scanf: %s", buf);
                                break;
                            }
                            MY_LOGW(TAG, "scanf error: %s", strerror(errno));
                            break;
                        }
                        total += ret;
                    }
                    // 处理每一个 client
                    for (LinkedListNode *p = client_fds.root; p != NULL; p = p->next) {
                        ret = send(p->data, "\rnotice: ", 9, 0);
                        ret = send(p->data, buf, total, 0);
                        ret = send(p->data, "> ", 2, 0);
                        if (ret < 0) {
                            MY_LOGW(TAG, "send error: %s", strerror(errno));
                            // 中途有客户端断连或其他错误 跳过
                            continue;
                        }
                        MY_LOGCI(TAG, "[%d] send: %s", p->data, buf);
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
                        MY_LOGI(TAG, "socket accept success. fd=%d, client_fd=%d", fd, client_fd);
                        ret = LinkedListAppend(&client_fds, client_fd);
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
                    memset(buf, 0, sizeof(buf));
                    total = 0;
                    while (total < buf_size) {
                        // int recv(描述符，空间，数据长度，标志位)
                        // 返回值：实际获取大小， 0-连接断开； -1-出错了
                        ret = recv(events[i].data.fd, buf + total, buf_size - total, 0);
                        if (ret < 0) {
                            /* 所有数据都读完了 */
                            if (errno == EAGAIN) {
                                MY_LOGCI(TAG, "[%d] recv msg: %s", events[i].data.fd, buf);
                                if (total <= 3) {
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
                                cJSON *response = openai_api_run(buf);
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
                            LinkedListRemove(&client_fds, events[i].data.fd, equal);
                            break;
                        }
                        total += ret;
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
