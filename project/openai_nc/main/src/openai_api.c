#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "curl/curl.h"
#include "my_log.h"

#define TAG "OPENAI_API"

// 初始化
CURLcode openai_api_init() {
    // Initialize curl
    return curl_global_init(CURL_GLOBAL_ALL);
}

void openai_api_deinit() {
    // Cleanup curl
    curl_global_cleanup();
}

// 定义写回调函数
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    // 计算字符串数组中有多少个字符
    size_t array_size = size * nmemb;

    // 定义字符串数组
    char *string_array = (char *)userp;
    // 把 `curl_easy_perform()` 函数的输出拷贝到字符串数组中
    if (array_size > 0) {
        memcpy(string_array, contents, array_size);
        string_array[array_size] = '\0';
    }

    // 返回字符串数组中字符的个数
    return array_size;
}

cJSON *openai_api_run(char *content) {
    char *buffer = (char *)malloc(4096 * sizeof(char));
    if (!buffer) {
        MY_LOGE(TAG, "Run out of memory: %d", __LINE__);
        return NULL;
    }

    // POST 数据
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "model", "text-davinci-003");
    cJSON_AddStringToObject(json, "prompt", content);
    cJSON_AddNumberToObject(json, "max_tokens", 4000);
    cJSON_AddNumberToObject(json, "temperature", 0.8);
    cJSON_AddNumberToObject(json, "top_p", 1.0);
    cJSON_AddNumberToObject(json, "frequency_penalty", 0.0);
    cJSON_AddNumberToObject(json, "presence_penalty", 0.0);
    char *post_data = cJSON_Print(json);

    CURL *curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/completions");
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        // api_key 通过全局变量读入
        char auth[100] = {0};
        char *api_key = getenv("OPENAI_API_KEY");
        if (api_key) {
            sprintf(auth, "Authorization: Bearer %s", api_key);
        } else {
            MY_LOGE(TAG, "Please set the environment variable `OPENAI_API_KEY` first\nYou can get it here: https://beta.openai.com/account/api-keys");
            return NULL;
        }
        headers = curl_slist_append(headers, auth);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        // 以秒为单位设置超时时间为30秒
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30);
        // 设置写回调函数
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        // 设置写回调函数的参数
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if (res != CURLE_OK) {
            MY_LOGE(TAG, "curl_easy_perform() failed: %s", curl_easy_strerror(res));
            free(post_data);
            free(buffer);
            cJSON_Delete(json);
            return NULL;
        }
    }
    free(post_data);
    cJSON_Delete(json);

    cJSON *response = cJSON_Parse(buffer);
    free(buffer);

    return response;
}
