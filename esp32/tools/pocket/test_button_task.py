"""Run the production button task with deterministic GPIO/timer/RTOS samples."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ButtonTaskTest(unittest.TestCase):
    def test_real_task_snapshots_threshold_and_reports_actual_hold(self):
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            for name, content in STUBS.items():
                path = temp / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content)
            harness = temp / "button_test.c"
            harness.write_text(HARNESS)
            for board in [0, 1]:
                binary = temp / ("button_" + str(board))
                subprocess.run([os.getenv("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                                "-Wno-unused-parameter", "-fsanitize=address,undefined",
                                "-DCONFIG_HOMEHUB_VOICE=0", "-DCONFIG_HOMEHUB_BUTTON_GPIO=0",
                                "-DCONFIG_HOMEHUB_LED_BACKEND_XTEINK_X4_PRO=" + str(board),
                                "-I", str(temp), "-I", str(ROOT / "main"), str(harness), "-o", str(binary)], check=True)
                subprocess.run([str(binary)], check=True)


STUBS = {
    "driver/gpio.h": r'''#pragma once
#include <stdint.h>
typedef int esp_err_t;
typedef struct {uint64_t pin_bit_mask;int mode,pull_up_en,pull_down_en,intr_type;} gpio_config_t;
enum {GPIO_MODE_INPUT,GPIO_PULLUP_ENABLE,GPIO_PULLDOWN_DISABLE,GPIO_INTR_DISABLE};
enum {ESP_OK=0};
int gpio_get_level(int pin);
int gpio_config(const gpio_config_t* config);
''',
    "freertos/FreeRTOS.h": '#pragma once\n#include <stddef.h>\n#define pdMS_TO_TICKS(n) (n)\n',
    "freertos/task.h": '#pragma once\nvoid vTaskDelay(int ms);\nint xTaskCreate(void(*task)(void*),const char*,int,void*,int,void*);\n',
    "esp_log.h": '#pragma once\n#define ESP_LOGI(tag, ...) ((void)(tag))\n#define ESP_LOGE(tag, ...) ((void)(tag))\n',
    "esp_timer.h": '#pragma once\n#include <stdint.h>\nint64_t esp_timer_get_time(void);\n',
}

HARNESS = r'''
#include <assert.h>
#include <setjmp.h>
#include <stddef.h>
#include "button.c"
typedef struct {int64_t time_ms;bool down;uint32_t threshold;} Sample;
static const Sample* samples;static size_t count,index_;
static jmp_buf stop;static int shorts,doubles,longs,selectors;static uint32_t held;
void stack_monitor_poll(stack_monitor_t* stack){(void)stack;}
int gpio_get_level(int pin){(void)pin;return samples[index_].down?0:1;}
int64_t esp_timer_get_time(void){return samples[index_].time_ms*1000;}
int gpio_config(const gpio_config_t* config){(void)config;return 0;}
int xTaskCreate(void(*task)(void*),const char* name,int stack,void* arg,int priority,void* handle){
 (void)task;(void)name;(void)stack;(void)arg;(void)priority;(void)handle;return 1;
}
void vTaskDelay(int ms){(void)ms;if(++index_==count)longjmp(stop,1);}
static void short_cb(void){++shorts;}
static void double_cb(void){++doubles;}
static void long_cb(void){++longs;held=button_long_press_duration_ms();}
static uint32_t selector(void){++selectors;return samples[index_].threshold;}
static void run(const Sample* data,size_t size){
 samples=data;count=size;index_=0;shorts=doubles=longs=selectors=0;held=0;
 button_set_long_press_threshold_cb(selector);assert(button_init(short_cb,double_cb,long_cb));
 if(!setjmp(stop))button_task(NULL);
}
int main(void){
 const Sample release[]={{0,true,2000},{1990,true,5000},{2010,false,5000},{3000,false,5000}};
 run(release,sizeof(release)/sizeof(*release));
#if CONFIG_HOMEHUB_LED_BACKEND_XTEINK_X4_PRO
 assert(longs==1 && held==2010 && shorts==0 && doubles==0 && selectors==1);
#else
 assert(longs==0 && shorts==0 && doubles==0 && selectors==0);
#endif
 const Sample setup[]={{0,true,5000},{2010,true,2000},{4990,true,2000},{5010,false,2000},{6000,false,2000}};
 run(setup,sizeof(setup)/sizeof(*setup));
 assert(longs==1 && held==5010 && shorts==0 && doubles==0);
#if CONFIG_HOMEHUB_LED_BACKEND_XTEINK_X4_PRO
 assert(selectors==1);
#else
 assert(selectors==0);
#endif
 const Sample once[]={{0,true,2000},{2000,true,2000},{6000,true,2000},{7000,false,2000},{8000,false,2000}};
 run(once,sizeof(once)/sizeof(*once));assert(longs==1 && shorts==0 && doubles==0);
#if CONFIG_HOMEHUB_LED_BACKEND_XTEINK_X4_PRO
 assert(held==2000);
#else
 assert(held==6000);
#endif
 const Sample tap[]={{0,true,2000},{100,false,2000},{500,false,2000}};
 run(tap,sizeof(tap)/sizeof(*tap));assert(shorts==1 && longs==0 && doubles==0);
 const Sample pairedgap[]={{0,true,2000},{1800,false,2000},{2200,false,2000}};
 run(pairedgap,sizeof(pairedgap)/sizeof(*pairedgap));
#if CONFIG_HOMEHUB_LED_BACKEND_XTEINK_X4_PRO
 assert(shorts==1 && longs==0);
#else
 assert(shorts==0 && longs==0);
#endif
 return 0;
}
'''


if __name__ == "__main__":
    unittest.main()
