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

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void (*button_cb)(void);
// X4 Pro samples this only on press-down: 2000ms calls Muse after setup,
// otherwise 5000ms retains the setup reset gesture. Set before button_init.
typedef uint32_t (*button_long_press_threshold_cb)(void);
void button_set_long_press_threshold_cb(button_long_press_threshold_cb cb);
// Elapsed duration of the most recent long event, available inside its callback.
uint32_t button_long_press_duration_ms(void);

bool button_init(button_cb on_short_press, button_cb on_double_press,
                 button_cb on_long_press);

// Called from the button task on each press and release. Returning true from
// a press claims it: it then counts as neither a tap nor a hold, and its
// release is reported too. Must not block.
typedef bool (*button_press_cb)(bool pressed);

void button_set_press_cb(button_press_cb cb);
