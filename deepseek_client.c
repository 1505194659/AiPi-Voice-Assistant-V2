#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "log.h"
#include "cJSON.h"
#include "https_client.h"
#include "config.h"

#define DBG_TAG "AI"

char* deepseek_chat(const char* input_text) {
    if (!input_text || strlen(input_text) == 0) {
        return NULL;
    }

    LOG_I("Asking DeepSeek: %s", input_text);

    // Construct JSON body
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", "deepseek-chat");
    cJSON_AddFalseToObject(root, "stream"); // Use non-streaming for simplicity first

    cJSON *messages = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "messages", messages);

    // Add system message to request short responses (for faster TTS)
    cJSON *sys_msg = cJSON_CreateObject();
    cJSON_AddItemToArray(messages, sys_msg);
    cJSON_AddStringToObject(sys_msg, "role", "system");
    cJSON_AddStringToObject(sys_msg, "content",
        "你是一个语音助手。请用简短的中文回复，不超过50个字。不要使用emoji或特殊符号。");

    cJSON *msg = cJSON_CreateObject();
    cJSON_AddItemToArray(messages, msg);
    cJSON_AddStringToObject(msg, "role", "user");
    cJSON_AddStringToObject(msg, "content", input_text);
    
    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!body) {
        LOG_E("Failed to create JSON body");
        return NULL;
    }
    
    int body_len = strlen(body);
    
    // Construct Headers
    char headers[256];
    sprintf(headers, 
        "Content-Type: application/json\r\n"
        "Authorization: Bearer %s\r\n", 
        DEEPSEEK_API_KEY);
        
    // Send Request
    char *response = https_request(DEEPSEEK_API_URL, "POST", headers, body, body_len);
    
    vPortFree(body);
    
    if (!response) {
        LOG_E("DeepSeek request failed");
        return NULL;
    }
    
    LOG_I("DeepSeek Response: %s", response);
    
    // Parse Response
    char *reply_text = NULL;
    
    // Find start of JSON object (skip chunk headers if present)
    char *json_start = strchr(response, '{');
    if (!json_start) {
        LOG_E("No JSON object found in response");
        vPortFree(response);
        return NULL;
    }
    
    // Find end of JSON object
    char *json_end = strrchr(response, '}');
    if (json_end) {
        *(json_end + 1) = '\0'; // Null terminate after the last '}'
    }
    
    LOG_I("Parsing JSON: %s", json_start);
    cJSON *json = cJSON_Parse(json_start);
    if (json) {
        cJSON *choices = cJSON_GetObjectItem(json, "choices");
        if (choices && cJSON_GetArraySize(choices) > 0) {
            cJSON *choice = cJSON_GetArrayItem(choices, 0);
            cJSON *message = cJSON_GetObjectItem(choice, "message");
            if (message) {
                cJSON *content = cJSON_GetObjectItem(message, "content");
                if (content && cJSON_IsString(content)) {
                    int len = strlen(content->valuestring);
                    reply_text = pvPortMalloc(len + 1);
                    strcpy(reply_text, content->valuestring);
                }
            }
        }
        cJSON_Delete(json);
    }
    
    vPortFree(response);
    return reply_text;
}
