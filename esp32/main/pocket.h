#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
// Call before storage/network initialization so recovery works without either.
void pocket_boot_guard(void);
bool pocket_recovery_available(void);
bool pocket_recovery_is_crossmux(void);
bool pocket_return_to_crosspoint(void);
// Local OTA health: initialized controls, first panel refresh, verified recovery.
bool pocket_local_boot_ready(void);
void pocket_image_begin(void);
void pocket_image_abort(void);
bool pocket_image_complete(void);
void pocket_set_status(const char* text);
// Separate immediate call overlay; ordinary Muse status never replaces it.
void pocket_set_call_state(const char* text);
// Up to 3072 UTF-8 bytes, paged inside the retained character area.
void pocket_show_call_result(const char* text);
// Call only when !pocket_call_active(): advances/dismisses results or errors.
bool pocket_result_next(void);
void pocket_set_frontlight(int brightness, int warmth);
const char* pocket_sdk_token(void);
#ifdef __cplusplus
}
#endif
