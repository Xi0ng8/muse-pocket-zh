// Original Muse Pocket placeholder. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
#include "sdkconfig.h"
#define HAPPY_ANIM_WIDTH 56
#define HAPPY_ANIM_HEIGHT 59
#define HAPPY_ANIM_FRAMES 1
#define HAPPY_ANIM_FRAME_MS 40
#if CONFIG_HOMEHUB_DISPLAY
extern const uint16_t happy_anim_palette[2];
extern const uint8_t happy_anim_frames[HAPPY_ANIM_FRAMES][HAPPY_ANIM_HEIGHT * HAPPY_ANIM_WIDTH];
#endif
