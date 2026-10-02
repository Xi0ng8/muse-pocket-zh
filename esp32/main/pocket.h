#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
// Call before storage/network initialization so recovery works without either.
void pocket_boot_guard(void);
bool pocket_recovery_available(void);
bool pocket_return_to_crosspoint(void);
// Local OTA health: initialized controls, first panel refresh, verified recovery.
bool pocket_local_boot_ready(void);
void pocket_image_begin(void);
void pocket_image_abort(void);
bool pocket_image_complete(void);
void pocket_set_status(const char* text);
void pocket_set_frontlight(int brightness, int warmth);
const char* pocket_sdk_token(void);
#ifdef __cplusplus
}
#endif
