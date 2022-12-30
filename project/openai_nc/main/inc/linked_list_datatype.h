#ifndef __LINKED_LIST_DATATYPE_H_
#define __LINKED_LIST_DATATYPE_H_

#define BUF_SIZE 4096

typedef struct {
    int fd;
    int buf_cap;
    int buf_len;
    char buffer[];
} FD_INFO;

#define LINKED_LIST_TYPE FD_INFO*

#endif
