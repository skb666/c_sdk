/*
 * convert.c
 *
 * Wrapper functions for ICU library to detect charset encodings
 * and convert from the deteced encoding to UTF8
 *
 * Copyright © 2015, AWeber Communications.
 * All rights reserved.
 */

#ifndef _CONVERT_H_
#define _CONVERT_H_

#include <stdbool.h>
#include <unicode/ucnv.h>
#include <unicode/ucnv_err.h>
#include <unicode/ucsdet.h>
#include <unicode/uloc.h>
#include <unicode/ustring.h>
#include <unicode/utypes.h>

#include "flagcb.h"

UErrorCode detect_ICU(const char* buffer, const char* hint,
                      char** encoding, char** lang, int32_t* confidence);

UErrorCode convert_to_unicode(const char* buffer, const char* encoding,
                              UChar** uBuf, int32_t* uBuf_len,
                              bool force, bool* dropped_bytes,
                              const int debug);

UErrorCode convert_to_utf8(const UChar* buffer, int32_t buffer_len,
                           char** converted_buf, int32_t* converted_buf_len,
                           bool force, bool* dropped_bytes,
                           const int debug);

#endif  // #ifndef _CONVERT_H_
