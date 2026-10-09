#include "pocket_call.h"
#include "noise_control.h"
#include "cJSON.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static bool paired=true,confirming=false,online=true,send_ok=true,open_ok=true,lock_allowed=true;
static int64_t now=0;static int opens=0,cancels=0,result_count=0;static unsigned random_seq=0;
static char state[256],result[3073],body[4096],call_id[37];
static noise_ctrl_req_cb callback;static void* callback_ctx;
void* xSemaphoreCreateMutex(void){return (void*)1;}
int xSemaphoreTake(void* m,unsigned t){(void)m;return lock_allowed || t == UINT32_MAX;}
int xSemaphoreGive(void* m){(void)m;return 1;}
int64_t esp_timer_get_time(void){return now;}
void esp_fill_random(void* p,size_t n){for(size_t i=0;i<n;i++)((unsigned char*)p)[i]=(unsigned char)(i+random_seq);random_seq++;}
bool config_setup_complete(void){return paired;}
bool config_is_provisioned(void){return paired;}
bool link_pairing_confirmation_required(void){return confirming;}
bool noise_ctrl_is_connected(void){return online;}
const char* noise_ctrl_device_id(void){return "test-node";}
void pocket_set_call_state(const char* s){snprintf(state,sizeof(state),"%s",s);}
void pocket_show_call_result(const char* s){snprintf(result,sizeof(result),"%s",s);result_count++;}
cJSON* cJSON_CreateObject(void){return calloc(1,sizeof(cJSON));}
cJSON* cJSON_AddStringToObject(cJSON* j,const char* k,const char* v){size_t n=strlen(j->data);snprintf(j->data+n,sizeof(j->data)-n,"%s=%s\n",k,v);return j;}
char* cJSON_PrintUnformatted(const cJSON* j){return strdup(j->data);}
void cJSON_Delete(cJSON* j){free(j);}
int64_t noise_ctrl_req_open(const char* verb,const char* path,const char* const* h,bool end,noise_ctrl_req_cb cb,void* ctx){
 assert(!strcmp(verb,"POST")&&!strcmp(path,"/chat/stream")&&!end);
 assert(!strcmp(h[0],"Content-Type")&&!strcmp(h[1],"application/json"));
 assert(!strcmp(h[2],"x-request-id")&&strlen(h[3])==36);snprintf(call_id,sizeof(call_id),"%s",h[3]);
 assert(!strcmp(h[4],"x-app-id")&&!strcmp(h[5],"musegadget")&&!h[6]);
 opens++;callback=cb;callback_ctx=ctx;return open_ok?16+opens:0;
}
bool noise_ctrl_req_send(int64_t id,const void* p,size_t n,bool end,int wait){assert(id>=16&&end&&wait==0&&n<sizeof(body));memcpy(body,p,n);body[n]=0;return send_ok;}
void noise_ctrl_req_cancel(int64_t id){assert(id>=16);cancels++;}
int main(void){
 pocket_call_start();pocket_call_tick();assert(opens==0&&!pocket_call_active());
 pocket_call_init();paired=false;pocket_call_start();assert(opens==0);
 paired=true;confirming=true;pocket_call_start();assert(opens==0);confirming=false;
 online=false;pocket_call_start();assert(opens==0&&!pocket_call_active()&&strstr(state,"离线"));online=true;
 pocket_call_start();assert(pocket_call_active()&&opens==1);assert(strstr(body,"output_modality=text")&&strstr(body,"device_id=test-node"));
 assert(strstr(body,"X4 Pro 左键预设")&&strstr(body,"pocket.complete_call")&&strstr(body,call_id));
 pocket_call_start();assert(opens==1);callback(callback_ctx,200,NULL,0,true);assert(pocket_call_active()&&result_count==0);
 assert(!pocket_call_complete("wrong","结果"));assert(!pocket_call_complete(call_id,""));assert(!pocket_call_complete(call_id,"\xc0\xaf"));
 char max[3074];memset(max,'x',3072);max[3072]=0;assert(pocket_call_complete(call_id,max)&&result_count==1&&!pocket_call_active());
 callback(callback_ctx,-1,NULL,0,true);assert(result_count==1&&strlen(result)==3072);assert(!pocket_call_complete(call_id,"重复"));
 noise_ctrl_req_cb old=callback;void* oldctx=callback_ctx;char oldid[37];strcpy(oldid,call_id);
 pocket_call_start();assert(opens==2&&pocket_call_active());old(oldctx,403,NULL,0,true);assert(pocket_call_active());assert(!pocket_call_complete(oldid,"迟到"));
 memset(max,'x',3073);max[3073]=0;assert(!pocket_call_complete(call_id,max));
 now+=180000001;pocket_call_tick();assert(!pocket_call_active()&&strstr(state,"未知"));assert(!pocket_call_complete(call_id,"迟到"));
 pocket_call_start();online=false;pocket_call_tick();assert(!pocket_call_active()&&strstr(state,"中断"));online=true;
 pocket_call_start();callback(callback_ctx,403,NULL,0,true);pocket_call_tick();assert(!pocket_call_active()&&strstr(state,"拒绝"));
 pocket_call_start();lock_allowed=false;callback(callback_ctx,-1,NULL,0,true);lock_allowed=true;pocket_call_tick();assert(!pocket_call_active()&&strstr(state,"中断"));
 send_ok=false;pocket_call_start();assert(!pocket_call_active()&&strstr(state,"发送失败"));send_ok=true;
 open_ok=false;pocket_call_start();assert(!pocket_call_active()&&strstr(state,"发送失败"));open_ok=true;
 pocket_call_start();assert(pocket_call_complete(call_id,"你好\n世界"));assert(!strcmp(result,"你好\n世界"));
 pocket_call_start();lock_allowed=false;assert(pocket_call_complete(call_id,"完成竞争测试"));lock_allowed=true;assert(!pocket_call_active());
 assert(cancels>0);return 0;
}
