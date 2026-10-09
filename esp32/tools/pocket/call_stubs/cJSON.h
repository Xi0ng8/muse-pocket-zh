#pragma once
typedef struct cJSON {char data[4096];} cJSON;
cJSON* cJSON_CreateObject(void);
cJSON* cJSON_AddStringToObject(cJSON*,const char*,const char*);
char* cJSON_PrintUnformatted(const cJSON*);
void cJSON_Delete(cJSON*);
