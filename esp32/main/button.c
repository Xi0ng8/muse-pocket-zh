/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "button.h"
#include "pocket_gesture.h"
#include "stack_monitor.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "link.button";

#define BTN_GPIO           CONFIG_HOMEHUB_BUTTON_GPIO
#define LONG_PRESS_MS      5000
#define SHORT_PRESS_MAX_MS 1000
#define POLL_MS            50

static button_cb s_short_press_cb = NULL;
static button_cb s_double_press_cb = NULL;
static button_cb s_long_press_cb = NULL;
static button_long_press_threshold_cb s_threshold_cb = NULL;
static uint32_t s_long_duration_ms;
void button_set_long_press_threshold_cb(button_long_press_threshold_cb cb) {
    s_threshold_cb = cb;
}
uint32_t button_long_press_duration_ms(void) {
    return s_long_duration_ms;
}
#if CONFIG_HOMEHUB_VOICE
static volatile button_press_cb s_press_cb = NULL;

void button_set_press_cb(button_press_cb cb) {
    s_press_cb = cb;
}
#endif

static void button_task(void *arg) {
    stack_monitor_t stack = STACK_MONITOR_INIT;
    pocket_gesture_t gesture = {0};
#if CONFIG_HOMEHUB_VOICE
    bool claimed = false;
#endif

    while (1) {
        bool pressed = (gpio_get_level(BTN_GPIO) == 0);

        bool was_pressed = gesture.pressed;
        uint32_t threshold = LONG_PRESS_MS;
        uint32_t short_max = SHORT_PRESS_MAX_MS;
#if CONFIG_HOMEHUB_LED_BACKEND_XTEINK_X4_PRO
        if (pressed && !was_pressed) {
            threshold = s_threshold_cb && s_threshold_cb() == 2000 ? 2000 : 5000;
            short_max = threshold == 2000 ? 1999 : SHORT_PRESS_MAX_MS;
        }
#endif
        int64_t now_ms = esp_timer_get_time() / 1000;
        pocket_gesture_event_t event = pocket_gesture_update(
            &gesture, pressed, now_ms, threshold, short_max);
        (void)was_pressed;
#if CONFIG_HOMEHUB_VOICE
        if (pressed && !was_pressed) {
            button_press_cb press_cb = s_press_cb;
            claimed = press_cb && press_cb(true);
            if (claimed) {
                // Not a tap or hold, and it ends any pending double click.
                pocket_gesture_cancel(&gesture);
                event = POCKET_GESTURE_NONE;
            }
        } else if (!pressed && was_pressed && claimed) {
            claimed = false;
            button_press_cb press_cb = s_press_cb;
            if (press_cb) press_cb(false);
            event = POCKET_GESTURE_NONE;
        }
#endif
        if (event == POCKET_GESTURE_LONG) {
            s_long_duration_ms = (uint32_t)(now_ms - gesture.press_ms);
            ESP_LOGI(TAG, "long press detected");
            if (s_long_press_cb) s_long_press_cb();
        } else if (event == POCKET_GESTURE_DOUBLE) {
            ESP_LOGI(TAG, "double press detected");
            if (s_double_press_cb) s_double_press_cb();
        } else if (event == POCKET_GESTURE_SHORT) {
            ESP_LOGI(TAG, "short press detected");
            if (s_short_press_cb) s_short_press_cb();
        }
        stack_monitor_poll(&stack);
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}

bool button_init(button_cb on_short_press, button_cb on_double_press,
                 button_cb on_long_press) {
    s_short_press_cb = on_short_press;
    s_double_press_cb = on_double_press;
    s_long_press_cb = on_long_press;

    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BTN_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio config failed: %s", esp_err_to_name(err));
        return false;
    }

    xTaskCreate(button_task, "btn", 4096, NULL, 2, NULL);
#if CONFIG_HOMEHUB_LED_BACKEND_XTEINK_X4_PRO
    ESP_LOGI(TAG, "button ready (GPIO %d: tap=next result, hold=2s Muse/5s setup)", BTN_GPIO);
#else
    ESP_LOGI(TAG, "button ready (GPIO %d: tap=retry wifi, 2x=rescan, hold %ds=reset setup)", BTN_GPIO, LONG_PRESS_MS / 1000);
#endif
    return true;
}
