// Copyright (c) Muse Pocket contributors. Licensed under Apache-2.0.
#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
void pocket_call_init(void);
void pocket_call_start(void);
void pocket_call_tick(void);
bool pocket_call_active(void);
// A completion must match the current UUID and contain 1..3072 valid UTF-8 bytes.
bool pocket_call_complete(const char* call_id, const char* text);
#ifdef __cplusplus
}
#endif
