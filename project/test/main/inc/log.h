/*
 * log.h
 *
 * Macros for logging with timestamp
 *
 * Copyright © 2015, AWeber Communications.
 * All rights reserved.
 */

#ifndef _LOG_H_
#define _LOG_H_

// pick up vasprintf
#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>

// log levels
#define FATAL "FATAL"
#define ERROR "ERROR"
#define WARNING "WARNING"
#define INFO "INFO"
#define DEBUG "DEBUG"

char* currentTimestamp();

void logger(FILE* dest, const char* level, const char* errcode,
            const char* msg, ...);

#define LOGSTDERR(level, errcode, msg, ...) \
    logger(stderr, level, errcode, msg, __VA_ARGS__)
#define LOGSTDOUT(level, errcode, msg, ...) \
    logger(stdout, level, errcode, msg, __VA_ARGS__)

#endif  // #ifndef _LOG_H_
