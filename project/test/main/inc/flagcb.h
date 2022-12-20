#ifndef _FLAGCB_H_
#define _FLAGCB_H_

#include <unicode/ucnv.h>
#include <unicode/ucnv_err.h>
#include <unicode/ucsdet.h>
#include <unicode/uloc.h>
#include <unicode/ustring.h>
#include <unicode/utypes.h>

// flag context
typedef struct
{
    UConverterToUCallback subCallback;
    const void *subContext;
    UBool flag;
} ToUFLAGContext;

typedef struct
{
    UConverterFromUCallback subCallback;
    const void *subContext;
    UBool flag;
} FromUFLAGContext;

// to Unicode context initializer
U_CAPI ToUFLAGContext *U_EXPORT2 flagCB_toU_openContext();

// from Unicode context initializer
U_CAPI FromUFLAGContext *U_EXPORT2 flagCB_fromU_openContext();

// to Unicode callback
U_CAPI void U_EXPORT2 flagCB_toU(
    const void *context,
    UConverterToUnicodeArgs *toUArgs,
    const char *codeUnits,
    int32_t length,
    UConverterCallbackReason reason,
    UErrorCode *err);

// from Unicode callback
U_CAPI void U_EXPORT2 flagCB_fromU(
    const void *context,
    UConverterFromUnicodeArgs *fromUArgs,
    const UChar *codeUnits,
    int32_t length,
    UChar32 codePoint,
    UConverterCallbackReason reason,
    UErrorCode *err);
#endif  // #ifdef _FLAGCB_H_
