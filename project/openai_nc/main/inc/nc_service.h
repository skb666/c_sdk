#ifndef __NC_SERVICE_H__
#define __NC_SERVICE_H__

#include <stdint.h>

int SocketCreate(const char *ip, uint16_t port);
void SocketAccept(int fd);

#endif
