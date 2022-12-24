#ifndef __LINKED_LIST_TYPEDEF_H_
#define __LINKED_LIST_TYPEDEF_H_

typedef struct {
    int fd;
    int total;
    char* pBuff;
} FdInfo_s;

#define LINKED_LIST_TYPE FdInfo_s

#endif