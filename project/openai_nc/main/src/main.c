#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "my_log.h"
#include "nc_service.h"
#include "openai_api.h"
#include "valid_ip.h"

#define TAG "MAIN"

void usage(char *arg) {
    printf(
        "usage: %s [-h <ip address>] [-p <port>]\n"
        "  -h   ipv4 address\n"
        "  -p   port\n",
        arg);
}

int main(int argc, char *argv[]) {
    openai_api_init();

    char *ip = "0.0.0.0";
    uint16_t port = 8888;

    int opt;
    while ((opt = getopt(argc, argv, "h:p:")) != -1) {
        switch (opt) {
            case 'h':
                ip = optarg;
                if (!ipv4_judge(optarg)) {
                    printf("Incorrect IP Address\n");
                    usage(argv[0]);
                    return -1;
                }
                break;
            case 'p':
                port = (uint16_t)atoi(optarg);
                break;
            case '?':
                usage(argv[0]);
                return -1;
        }
    }

    int fd = SocketCreate(ip, port);
    MY_LOGCI(TAG, "NCService run at %s:%hu\n", ip, port);
    if (fd == -1) {
        MY_LOGCI(TAG, "socket create fd failed\n");
        return -1;
    }
    SocketAccept(fd);

    openai_api_deinit();

    return 0;
}
