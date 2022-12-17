#ifndef __OPENAI_API_H__
#define __OPENAI_API_H__

#include "cJSON.h"
#include "curl/curl.h"

CURLcode openai_api_init();
void openai_api_deinit();
cJSON *openai_api_run(char *content);

#endif
