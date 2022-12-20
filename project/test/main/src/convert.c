/*
 * convert.c
 *
 * Wrapper functions for ICU library to detect charset encodings
 * and convert from the deteced encoding to UTF8
 *
 * Copyright © 2015, AWeber Communications.
 * All rights reserved.
 */

// pick up vasprintf
#define _GNU_SOURCE
#include "convert.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

// ICU includes
#include "unicode/ucnv.h"
#include "unicode/ucnv_err.h"
#include "unicode/ucsdet.h"
#include "unicode/uloc.h"
#include "unicode/ustring.h"

#define STRING_IS_NULL_TERMINATED -1

// detect the charset encoding of a NUL terminated C string
UErrorCode
detect_ICU(const char* buffer, const char* hint, char** encoding, char** lang, int32_t* confidence) {
    UCharsetDetector* csd;
    const UCharsetMatch* csm;
    UErrorCode status = U_ZERO_ERROR;

    csd = ucsdet_open(&status);

    // set "hint" encoding if given
    if (hint) {
        ucsdet_setDeclaredEncoding(csd, hint, STRING_IS_NULL_TERMINATED, &status);

        if (U_FAILURE(status)) {
            LOGSTDERR(
                ERROR,
                u_errorName(status),
                "ICU error: %s\nResetting detector.",
                u_errorName(status));

            // make sure the detector is reset
            ucsdet_close(csd);
            csd = ucsdet_open(&status);
        }
    }

    // set conversion string buffer
    // use -1 for string length since NUL terminated
    ucsdet_setText(csd, buffer, STRING_IS_NULL_TERMINATED, &status);

    // detect charset
    csm = ucsdet_detect(csd, &status);

    // charset match is NULL if no match
    if (NULL == csm) {
        LOGSTDERR(
            WARNING,
            u_errorName(status),
            "ICU error: Detection failed for \"%s\".",
            buffer);

        *encoding = NULL;
        *lang = NULL;
        *confidence = 0;
    } else if (U_FAILURE(status)) {
        LOGSTDERR(
            ERROR,
            u_errorName(status),
            "ICU error: %s\n",
            u_errorName(status));

        *encoding = NULL;
        *lang = NULL;
        *confidence = 0;
    } else {
        *encoding = (char*)ucsdet_getName(csm, &status);
        *lang = (char*)ucsdet_getLanguage(csm, &status);
        *confidence = ucsdet_getConfidence(csm, &status);
    }

    // close charset detector
    // UCharsetMatch is owned by UCharsetDetector so its memory will be
    // freed when the char set detector is closed
    ucsdet_close(csd);
    return status;
}

// Convert from detected encoding to ICU internal Unicode

UErrorCode
convert_to_unicode(const char* buffer, const char* encoding,
                   UChar** uBuf, int32_t* uBuf_len,
                   bool force, bool* dropped_bytes,
                   const int debug) {
    UErrorCode status = U_ZERO_ERROR;

    UConverter* conv;
    int32_t uConvertedLen = 0;

    // used to set dropped_bytes flag if force is true
    ToUFLAGContext* context = NULL;

    size_t uBufSize = 0;

    // open converter for detected encoding
    conv = ucnv_open(encoding, &status);

    if (U_FAILURE(status)) {
        LOGSTDERR(
            ERROR,
            u_errorName(status),
            "ICU error - cannot open %s converter.\n",
            encoding);

        ucnv_close(conv);
        return status;
    }

    if (force) {
        // set callback to skip illegal, irregular or unassigned bytes

        // set converter to use SKIP callback
        // contecxt will save and call it after calling custom callback
        ucnv_setToUCallBack(conv,
                            UCNV_TO_U_CALLBACK_SKIP,
                            NULL,  // context
                            NULL,  // subcallback
                            NULL,  // subcontent
                            &status);

        if (U_FAILURE(status)) {
            LOGSTDERR(
                ERROR,
                u_errorName(status),
                "ICU error - cannot set SKIP callback for %s converter.\n",
                encoding);

            ucnv_close(conv);
            return status;
        }

        // initialize flagging callback
        context = flagCB_toU_openContext();

        /* Set our special callback */
        ucnv_setToUCallBack(conv,
                            flagCB_toU,
                            context,
                            &(context->subCallback),
                            &(context->subContext),
                            &status);

        if (U_FAILURE(status)) {
            LOGSTDERR(
                ERROR,
                u_errorName(status),
                "ICU error - cannot set FLAG callback for %s converter.\n",
                encoding);

            ucnv_close(conv);
            return status;
        }
    }

    // allocate unicode buffer
    // must free before exiting calling function
    uBufSize = (strlen(buffer) / ucnv_getMinCharSize(conv) + 1);
    *uBuf = (UChar*)calloc(sizeof(UChar), uBufSize * sizeof(UChar));

    if (*uBuf == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;

        LOGSTDERR(
            ERROR,
            u_errorName(status),
            "ICU error - cannot allocate %d bytes for Unicode pivot buffer.\n",
            (int)uBufSize);

        ucnv_close(conv);
        return status;
    }

    if (debug)
        LOGSTDERR(DEBUG, u_errorName(status), "Original string: %s\n", buffer);

    // convert to Unicode
    // returns length of converted string, not counting NUL-terminator
    uConvertedLen = ucnv_toUChars(conv,
                                  *uBuf,
                                  uBufSize,
                                  buffer,
                                  STRING_IS_NULL_TERMINATED,
                                  &status);

    if (U_SUCCESS(status)) {
        // add 1 for NUL terminator
        *uBuf_len = uConvertedLen + 1;

        // see if any bytes where dropped
        // context struct will go away with converter is closed
        if (force)
            *dropped_bytes = context->flag;
        else
            *dropped_bytes = false;
    }

    if (U_FAILURE(status)) {
        LOGSTDERR(ERROR, u_errorName(status),
                  "ICU conversion to Unicode failed for: %s\n", encoding);
    }

    ucnv_close(conv);
    return status;
}

// Convert from ICU internal Unicode to UTF8

UErrorCode
convert_to_utf8(const UChar* buffer, int32_t buffer_len,
                char** converted_buf, int32_t* converted_buf_len,
                bool force, bool* dropped_bytes,
                const int debug) {
    UConverter* conv;
    UErrorCode status = U_ZERO_ERROR;
    int32_t utfConvertedLen = 0;

    // used to set dropped_bytes flag if force is true
    FromUFLAGContext* context = NULL;

    // open UTF8 converter
    conv = ucnv_open("utf-8", &status);

    if (U_FAILURE(status)) {
        LOGSTDERR(ERROR, u_errorName(status),
                  "ICU error - Cannot open utf-8 converter.\n", NULL);

        ucnv_close(conv);
        return status;
    }

    if (force) {
        // set callback to skip illegal, irregular or unassigned bytes

        // set converter to use SKIP callback
        // contecxt will save and call it after calling custom callback
        ucnv_setFromUCallBack(conv,
                              UCNV_FROM_U_CALLBACK_SKIP,
                              NULL,  // context
                              NULL,  // subcallback
                              NULL,  // subcontent
                              &status);

        if (U_FAILURE(status)) {
            LOGSTDERR(
                ERROR,
                u_errorName(status),
                "ICU error - cannot set SKIP callback for UTF8 converter.\n",
                NULL);

            ucnv_close(conv);
            return status;
        }

        // initialize flagging callback
        context = flagCB_fromU_openContext();

        /* Set our special callback */
        ucnv_setFromUCallBack(conv,
                              flagCB_fromU,
                              context,
                              &(context->subCallback),
                              &(context->subContext),
                              &status);

        if (U_FAILURE(status)) {
            LOGSTDERR(ERROR, u_errorName(status),
                      "ICU error - cannot set FLAG callback for UTF8 converter.\n",
                      NULL);

            ucnv_close(conv);
            return status;
        }
    }

    // convert to UTF8
    // input buffer from ucnv_toUChars, which always returns a
    // NUL-terminated buffer
    utfConvertedLen = ucnv_fromUChars(conv,
                                      *converted_buf,
                                      *converted_buf_len,
                                      buffer,
                                      STRING_IS_NULL_TERMINATED,
                                      &status);

    if (U_SUCCESS(status)) {
        *converted_buf_len = utfConvertedLen;

        if (debug)
            LOGSTDERR(INFO, u_errorName(status),
                      "Converted string %s\n", (const char*)*converted_buf);

        // see if any bytes where dropped
        // context struct will go away when converter is closed
        if (force && NULL != context)
            *dropped_bytes = context->flag;
        else
            *dropped_bytes = false;
    }

    if (U_FAILURE(status)) {
        LOGSTDERR(
            ERROR,
            u_errorName(status),
            "ICU error - conversion from Unicode to UTF8 failed.\n",
            NULL);
    }

    // close the converter
    ucnv_close(conv);
    return status;
}
