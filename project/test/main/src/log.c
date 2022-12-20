/*
 * log.c
 *
 * Logging with timestamp
 *
 * Copyright © 2015, AWeber Communications.
 * All rights reserved.
 */

#define _GNU_SOURCE
#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

char* currentTimestamp(char* tsbuf, size_t buflen) {
    struct timeval tv;
    struct tm nowtm;
    size_t tmlen;

    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &nowtm);
    tmlen = strftime(tsbuf, buflen, "%F %T", &nowtm);
    snprintf(&tsbuf[tmlen], buflen - tmlen, ".%.06ld", tv.tv_usec);
    return tsbuf;
}

void logger(FILE* dest, const char* level, const char* errcode,
            const char* msg, ...) {
    char tsbuf[28] = {0};
    va_list args;

    currentTimestamp(tsbuf, sizeof(tsbuf));
    va_start(args, msg);
    fprintf(dest, "%s %s: %s - ", tsbuf, level, errcode);
    vfprintf(dest, msg, args);
    fprintf(dest, "\n");
    va_end(args);
    fflush(NULL);
}
