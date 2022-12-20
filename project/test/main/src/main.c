#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "convert.h"
#include "log.h"

int main() {
    // encoding, language & confidence level
    char *encoding = NULL;
    char *lang = NULL;
    int32_t confidence = 0;
    // ICU status error code
    UErrorCode uStatus = U_ZERO_ERROR;

    char buffer[1001];
    scanf("%1000[^\n]", buffer);
    // detect encoding with ICU
    uStatus = detect_ICU(buffer, NULL, &encoding, &lang, &confidence);
    if (U_FAILURE(uStatus) || encoding == NULL) {
        LOGSTDERR(ERROR, u_errorName(uStatus),
                  "Charset detection of failed - aborting transcoding.\n", NULL);

        encoding = NULL;
        lang = NULL;
        confidence = 0;
    }

    LOGSTDERR(DEBUG, u_errorName(uStatus),
              "ICU detection status: %d\n", uStatus);
    LOGSTDERR(DEBUG, u_errorName(uStatus),
              "Detected encoding: %s, language: %s, confidence: %d\n",
              encoding, lang, confidence);

    if (
        (0 == strcmp("UTF-8", encoding)) ||
        (0 == strcmp("utf-8", encoding)) ||
        (0 == strcmp("UTF8", encoding)) ||
        (0 == strcmp("utf8", encoding))) {
    } else {
        // temporary buffer for converted string
        char *converted_buf = NULL;
        // ICU variables of holding
        UChar *uBuf = NULL;
        int32_t uBuf_len = 0;
        // dropped bytes going to UTF16?
        bool dropped_bytes_toU = false;
        // dropped bytes going to UTF8?
        bool dropped_bytes_fromU = false;
        // UTF8 output can be up to 6 bytes per input byte
        int32_t converted_buf_len = strlen(buffer) * 6 * sizeof(char);
        converted_buf = (char *)malloc(converted_buf_len + 1);
        memset(converted_buf, 0, converted_buf_len + 1);

        // ICU uses UTF16 internally, so need to convert to UTF16 first
        // then convert to UTF8
        if (U_SUCCESS(uStatus))
            uStatus = convert_to_unicode(buffer, encoding,
                                         &uBuf, (int32_t *)&uBuf_len,
                                         true, &dropped_bytes_toU, 1);

        LOGSTDERR(DEBUG, u_errorName(uStatus),
                  "ICU conversion to Unicode status: %d\n", uStatus);

        // so far so good. convert from UTF16 to UTF8
        if (U_SUCCESS(uStatus))
            uStatus = convert_to_utf8((const UChar *)uBuf, uBuf_len,
                                      &converted_buf, (int32_t *)&converted_buf_len,
                                      true, &dropped_bytes_fromU, 1);

        free((void *)uBuf);

        LOGSTDERR(DEBUG, u_errorName(uStatus),
                  "ICU conversion to UTF8 status: %d\n", uStatus);

        // ICU thinks it worked!
        if (U_SUCCESS(uStatus)) {
            LOGSTDERR(DEBUG, u_errorName(uStatus),
                      "ICU conversion complete - status: %d\n", uStatus);

            printf("%s\n", converted_buf);
            // *converted = true;
            // *dropped_bytes = (dropped_bytes_toU || dropped_bytes_fromU);

            // return converted buffer
            // return (const char *)converted_buf;
        } else {
            LOGSTDERR(ERROR, u_errorName(uStatus),
                      "ICU conversion failed; returning original input - status: %d\n", uStatus);

            printf("%s\n", buffer);
            // *converted = false;
            // *dropped_bytes = false;

            // return original buffer
            // return strdup(buffer);
        }
    }

    return 0;
}
