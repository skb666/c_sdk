#include "flagcb.h"

// pick up vasprintf
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG_TMI 0 /* set to 1 for Too Much Information (TMI) */

U_CAPI ToUFLAGContext *U_EXPORT2 flagCB_toU_openContext() {
    ToUFLAGContext *ctx;

    ctx = (ToUFLAGContext *)malloc(sizeof(ToUFLAGContext));

    ctx->subCallback = NULL;
    ctx->subContext = NULL;
    ctx->flag = false;

    return ctx;
}

U_CAPI FromUFLAGContext *U_EXPORT2 flagCB_fromU_openContext() {
    FromUFLAGContext *ctx;

    ctx = (FromUFLAGContext *)malloc(sizeof(FromUFLAGContext));

    ctx->subCallback = NULL;
    ctx->subContext = NULL;
    ctx->flag = false;

    return ctx;
}

U_CAPI void U_EXPORT2 flagCB_toU(
    const void *context,
    UConverterToUnicodeArgs *toUArgs,
    const char *codeUnits,
    int32_t length,
    UConverterCallbackReason reason,
    UErrorCode *err) {
    /* First step - based on the reason code, take action */

    if ((UCNV_UNASSIGNED == reason) ||
        (UCNV_ILLEGAL == reason) ||
        (UCNV_IRREGULAR == reason))
        ((ToUFLAGContext *)context)->flag = true;

    if (UCNV_CLONE == reason) {
        /* The following is the recommended way to implement UCNV_CLONE
           in a callback. */
        UConverterToUCallback saveCallback;
        const void *saveContext;
        ToUFLAGContext *old, *cloned;
        UErrorCode subErr = U_ZERO_ERROR;

#if DEBUG_TMI
        printf("*** FLAGCB: cloning %p ***\n", context);
#endif
        old = (ToUFLAGContext *)context;
        cloned = flagCB_toU_openContext();

        memcpy(cloned, old, sizeof(ToUFLAGContext));

#if DEBUG_TMI
        printf("%p: my subcb=%p:%p\n", old, old->subCallback,
               old->subContext);
        printf("%p: cloned subcb=%p:%p\n", cloned, cloned->subCallback,
               cloned->subContext);
#endif

        /* We need to get the sub CB to handle cloning,
         * so we have to set up the following, temporarily:
         *
         *   - Set the callback+context to the sub of this (flag) cb
         *   - preserve the current cb+context, it could be anything
         *
         *   Before:
         *      CNV  ->   FLAG ->  subcb -> ...
         *
         *   After:
         *      CNV  ->   subcb -> ...
         *
         *    The chain to 'something' on is saved, and will be restored
         *   at the end of this block.
         *
         */

        ucnv_setToUCallBack(toUArgs->converter,
                            cloned->subCallback,
                            cloned->subContext,
                            &saveCallback,
                            &saveContext,
                            &subErr);

        if (cloned->subCallback != NULL)
            /* Now, call the sub callback if present */
            cloned->subCallback(cloned->subContext, toUArgs, codeUnits,
                                length, reason, err);

        ucnv_setToUCallBack(toUArgs->converter,
                            saveCallback,         /* Us */
                            cloned,               /* new context */
                            &cloned->subCallback, /* IMPORTANT! Accept any change in CB or context */
                            &cloned->subContext,
                            &subErr);

        if (U_FAILURE(subErr))
            *err = subErr;
    }

    /* process other reasons here if need be */

    /* Always call the subCallback if present */
    if (((ToUFLAGContext *)context)->subCallback != NULL && reason != UCNV_CLONE)
        ((ToUFLAGContext *)context)->subCallback(((ToUFLAGContext *)context)->subContext, toUArgs, codeUnits, length, reason, err);

    /* cleanup - free the memory AFTER calling the sub CB */
    if (reason == UCNV_CLOSE)
        free((void *)context);
}

// from Unicode callback
U_CAPI void U_EXPORT2 flagCB_fromU(
    const void *context,
    UConverterFromUnicodeArgs *fromUArgs,
    const UChar *codeUnits,
    int32_t length,
    UChar32 codePoint,
    UConverterCallbackReason reason,
    UErrorCode *err) {
    /* First step - based on the reason code, take action */

    if ((UCNV_UNASSIGNED == reason) ||
        (UCNV_ILLEGAL == reason) ||
        (UCNV_IRREGULAR == reason))
        ((FromUFLAGContext *)context)->flag = true;

    if (reason == UCNV_CLONE) {
        /* The following is the recommended way to implement UCNV_CLONE
           in a callback. */
        UConverterFromUCallback saveCallback;
        const void *saveContext;
        FromUFLAGContext *old, *cloned;
        UErrorCode subErr = U_ZERO_ERROR;

#if DEBUG_TMI
        printf("*** FLAGCB: cloning %p ***\n", context);
#endif
        old = (FromUFLAGContext *)context;
        cloned = flagCB_fromU_openContext();

        memcpy(cloned, old, sizeof(FromUFLAGContext));

#if DEBUG_TMI
        printf("%p: my subcb=%p:%p\n", old, old->subCallback,
               old->subContext);
        printf("%p: cloned subcb=%p:%p\n", cloned, cloned->subCallback,
               cloned->subContext);
#endif

        /* We need to get the sub CB to handle cloning,
         * so we have to set up the following, temporarily:
         *
         *   - Set the callback+context to the sub of this (flag) cb
         *   - preserve the current cb+context, it could be anything
         *
         *   Before:
         *      CNV  ->   FLAG ->  subcb -> ...
         *
         *   After:
         *      CNV  ->   subcb -> ...
         *
         *    The chain from 'something' on is saved, and will be restored
         *   at the end of this block.
         *
         */

        ucnv_setFromUCallBack(fromUArgs->converter,
                              cloned->subCallback,
                              cloned->subContext,
                              &saveCallback,
                              &saveContext,
                              &subErr);

        if (cloned->subCallback != NULL)
            /* Now, call the sub callback if present */
            cloned->subCallback(cloned->subContext, fromUArgs, codeUnits,
                                length, codePoint, reason, err);

        ucnv_setFromUCallBack(fromUArgs->converter,
                              saveCallback,         /* Us */
                              cloned,               /* new context */
                              &cloned->subCallback, /* IMPORTANT! Accept any change in CB or context */
                              &cloned->subContext,
                              &subErr);

        if (U_FAILURE(subErr))
            *err = subErr;
    }

    /* process other reasons here if need be */

    /* Always call the subCallback if present */
    if (((FromUFLAGContext *)context)->subCallback != NULL && reason != UCNV_CLONE)
        ((FromUFLAGContext *)context)->subCallback(((FromUFLAGContext *)context)->subContext, fromUArgs, codeUnits, length, codePoint, reason, err);

    /* cleanup - free the memory AFTER calling the sub CB */
    if (reason == UCNV_CLOSE)
        free((void *)context);
}
