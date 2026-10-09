#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
bool noise_ctrl_is_connected(void);
const char* noise_ctrl_device_id(void);
typedef void(*noise_ctrl_req_cb)(void*,int,const uint8_t*,size_t,bool);
int64_t noise_ctrl_req_open(const char*,const char*,const char* const*,bool,noise_ctrl_req_cb,void*);
bool noise_ctrl_req_send(int64_t,const void*,size_t,bool,int);
void noise_ctrl_req_cancel(int64_t);
