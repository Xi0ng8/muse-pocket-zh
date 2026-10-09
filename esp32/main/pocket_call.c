// Copyright (c) Muse Pocket contributors. Licensed under Apache-2.0.
#include "pocket_call.h"
#include "pocket.h"
#include "noise_control.h"
#include "config_store.h"
#include "link_pairing.h"
#include "cJSON.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CALL_WAIT_US (180LL * 1000 * 1000)
static SemaphoreHandle_t s_lock;
static uintptr_t s_generation;
// One packed generation/error mailbox: callbacks neither wait for a mutex nor
// enter the display renderer. Current errors replace obsolete generations.
static atomic_uintptr_t s_callback_owner;
static atomic_uintptr_t s_failure;
static bool s_active;
static int64_t s_request;
static int64_t s_deadline;
static char s_uuid[37];

static const char* const unknown = "等待超时，任务状态未知；请在 Muse 中查看。";
static const char* const interrupted = "连接中断，呼叫未完成；任务状态未知。";

static bool take(void) { return s_lock && xSemaphoreTake(s_lock, pdMS_TO_TICKS(10)) == pdTRUE; }
static void give(void) { xSemaphoreGive(s_lock); }

void pocket_call_init(void) { if (!s_lock) s_lock = xSemaphoreCreateMutex(); }

bool pocket_call_active(void) {
    if (!s_lock) return false;
    if (!take()) return true; // Never dismiss an overlay while a transition owns it.
    bool active = s_active;
    give();
    return active;
}

// No request allocates a callback context: the generation is an integer value.
// A cancelled stream can disappear without a final callback, or call back late.
static void response(void* ctx, int status, const uint8_t* data, size_t len, bool end) {
    (void)data; (void)len; (void)end;
    if (status >= 0 && status < 300) return;
    uintptr_t generation = (uintptr_t)ctx;
    uintptr_t event = (generation << 2) | (status < 0 ? 1u : 2u);
    for (;;) {
        uintptr_t previous = atomic_load(&s_failure);
        if (atomic_load(&s_callback_owner) != generation) return;
        if ((previous >> 2) == generation) return;
        if (atomic_compare_exchange_weak(&s_failure, &previous, event)) return;
    }
}

void pocket_call_start(void) {
    if (!take()) return;
    if (s_active || !config_setup_complete() || !config_is_provisioned() || link_pairing_confirmation_required()) {
        give(); return;
    }
    if (!noise_ctrl_is_connected()) {
        pocket_set_call_state("Muse 当前离线，未发送呼叫。");
        give(); return;
    }
    const char* device_id = noise_ctrl_device_id();
    if (!device_id || !*device_id) {
        pocket_set_call_state("发送失败，设备身份尚未就绪。");
        give(); return;
    }
    uint8_t random[16];
    esp_fill_random(random, sizeof(random));
    random[6] = (random[6] & 15) | 64;
    random[8] = (random[8] & 63) | 128;
    snprintf(s_uuid, sizeof(s_uuid),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        random[0],random[1],random[2],random[3],random[4],random[5],random[6],random[7],
        random[8],random[9],random[10],random[11],random[12],random[13],random[14],random[15]);
    s_generation = (s_generation + 1) & (UINTPTR_MAX >> 2);
    if (!s_generation) ++s_generation;
    atomic_store(&s_failure, 0);
    atomic_store(&s_callback_owner, s_generation);
    char message[1200];
    snprintf(message, sizeof(message),
        "执行你在 Muse 内为用户持久保存的『X4 Pro 左键预设』。按该预设及用户原有权限执行；"
        "不要从最近聊天猜测任务，不要增加未经授权的操作。若找不到预设，返回请在 Muse 中设置"
        "『X4 Pro 左键预设』的提示。完成后必须调用 pocket.complete_call，call_id 为 %s，"
        "text 是最终结果或明确失败说明，1..3072 UTF-8 字节，适合黑白屏的简短段落，不使用 emoji。"
        "不要只更新 pocket.set_status；只有匹配 call_id 的 complete_call 才表示本次呼叫完成。", s_uuid);
    cJSON* json = cJSON_CreateObject();
    char* body = NULL;
    if (json && cJSON_AddStringToObject(json, "message", message) &&
        cJSON_AddStringToObject(json, "output_modality", "text") &&
        cJSON_AddStringToObject(json, "device_id", device_id)) body = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!body) {
        atomic_store(&s_callback_owner, 0);
        pocket_set_call_state("发送失败，请稍后再试。");
        give(); return;
    }
    const char* headers[] = {"Content-Type", "application/json", "x-request-id", s_uuid,
                             "x-app-id", "musegadget", NULL};
    s_active = true;
    s_deadline = esp_timer_get_time() + CALL_WAIT_US;
    pocket_set_call_state("正在呼叫 Muse，请等待预设结果。");
    s_request = noise_ctrl_req_open("POST", "/chat/stream", headers, false, response, (void*)s_generation);
    bool sent = s_request && noise_ctrl_req_send(s_request, body, strlen(body), true, 0);
    free(body);
    int64_t cancel = 0;
    if (!sent) {
        cancel = s_request;
        s_request = 0;
        s_active = false;
        atomic_store(&s_callback_owner, 0);
        pocket_set_call_state("发送失败，任务可能尚未开始；请在 Muse 中查看。");
    }
    give();
    if (cancel) noise_ctrl_req_cancel(cancel);
}

void pocket_call_tick(void) {
    if (!take()) return;
    uintptr_t failure = atomic_exchange(&s_failure, 0);
    bool current_error = (failure >> 2) == s_generation && (failure & 3u) != 0;
    int64_t cancel = 0;
    if (s_active && (current_error || !noise_ctrl_is_connected() || esp_timer_get_time() >= s_deadline)) {
        const char* state = current_error && (failure & 3u) == 2u ? "Muse 拒绝呼叫，请检查权限或预设。" :
                            current_error || !noise_ctrl_is_connected() ? interrupted : unknown;
        s_active = false;
        atomic_store(&s_callback_owner, 0);
        cancel = s_request;
        s_request = 0;
        pocket_set_call_state(state);
    }
    give();
    if (cancel) noise_ctrl_req_cancel(cancel);
}

static bool valid_text(const char* text) {
    if (!text) return false;
    size_t length = strnlen(text, 3073);
    if (!length || length > 3072) return false;
    for (size_t i = 0; i < length;) {
        uint32_t cp = (unsigned char)text[i++];
        unsigned extra = 0; uint32_t minimum = 0;
        if (cp < 128) { if (cp < 32 && cp != 10 && cp != 9) return false; continue; }
        if (cp >= 0xc2 && cp <= 0xdf) { cp &= 31; extra = 1; minimum = 0x80; }
        else if (cp >= 0xe0 && cp <= 0xef) { cp &= 15; extra = 2; minimum = 0x800; }
        else if (cp >= 0xf0 && cp <= 0xf4) { cp &= 7; extra = 3; minimum = 0x10000; }
        else return false;
        if (length - i < extra) return false;
        while (extra--) {
            unsigned char next = (unsigned char)text[i++];
            if ((next & 0xc0) != 0x80) return false;
            cp = (cp << 6) | (next & 63);
        }
        if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
    }
    return true;
}

bool pocket_call_complete(const char* call_id, const char* text) {
    if (!call_id || strnlen(call_id, 37) != 36 || !valid_text(text) || !s_lock) return false;
    // Completion is a definitive server event: do not drop it while a display
    // transition owns the state lock. UI callers never hold their lock here.
    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) return false;
    bool matched = s_active && strcmp(call_id, s_uuid) == 0 && esp_timer_get_time() < s_deadline;
    int64_t cancel = 0;
    if (matched) {
        s_active = false;
        atomic_store(&s_callback_owner, 0);
        cancel = s_request;
        s_request = 0;
        pocket_show_call_result(text); // UI copies the bounded text before return.
    }
    give();
    if (cancel) noise_ctrl_req_cancel(cancel);
    return matched;
}
