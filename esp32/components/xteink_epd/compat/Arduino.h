#pragma once
// The small Arduino surface used by the MIT panel drivers, backed by ESP-IDF.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define PROGMEM
inline uint8_t pgm_read_byte(const void* p) { return *static_cast<const uint8_t*>(p); }
inline void delay(unsigned long ms) { vTaskDelay(pdMS_TO_TICKS(ms) ? pdMS_TO_TICKS(ms) : 1); }
inline void delayMicroseconds(unsigned int us) { esp_rom_delay_us(us); }
inline unsigned long millis() { return esp_timer_get_time() / 1000; }
inline void digitalWrite(int pin, int value) { gpio_set_level(static_cast<gpio_num_t>(pin), value); }
inline int digitalRead(int pin) { return gpio_get_level(static_cast<gpio_num_t>(pin)); }
inline void pinMode(int pin, int mode) {
    gpio_config_t c = {};
    c.pin_bit_mask = 1ULL << pin;
    c.mode = mode == OUTPUT ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT;
    c.pull_up_en = mode == INPUT_PULLUP ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    gpio_config(&c);
}
