#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef unsigned TickType_t;
#define pdTRUE 1
#define pdMS_TO_TICKS(x) (x)

#define portMAX_DELAY UINT32_MAX
