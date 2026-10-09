"""Compile and execute app.c's actual button callbacks with control doubles."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class CallRoutesTests(unittest.TestCase):
    def test_setup_pairing_busy_and_held_time_routes(self):
        app = (ROOT / "main/app.c").read_text()
        start = app.index("static void on_button_short_press(void) {")
        end = app.index("\n\n#if CONFIG_MUSE_ENABLED", start)
        callbacks = app[start:end]
        doubles = r'''
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#define CONFIG_HOMEHUB_LED_BACKEND_XTEINK_X4_PRO 1
#define ESP_LOGI(...) ((void)0)
static bool setup, confirm, ble, busy;
static uint32_t held, generation;
static unsigned calls, resets, pages, windows, confirms, factory;
static bool config_setup_complete(void) { return setup; }
static bool link_pairing_confirmation_required(void) { return confirm; }
static bool ble_server_has_connection(void) { return ble; }
static uint32_t link_pairing_confirm_active_session(void) { return generation; }
static void notify_pairing_confirmed(uint32_t g) { assert(g == generation); confirms++; }
static void factory_test_on_button_press(void) { factory++; }
static void open_setup_window(const char* reason) { assert(reason); windows++; }
static bool pocket_call_active(void) { return busy; }
static bool pocket_result_next(void) { pages++; return true; }
static void pocket_call_start(void) { calls++; }
static uint32_t button_long_press_duration_ms(void) { return held; }
static void reset_setup_from_control(const char* reason) { assert(reason); resets++; }
'''
        assertions = r'''
int main(void) {
 assert(pocket_button_hold_threshold() == 5000);
 held = 4999; on_button_long_press(); assert(!resets && !calls);
 held = 5000; on_button_long_press(); assert(resets == 1 && !calls);
 on_button_short_press(); assert(windows == 1 && !pages);
 setup = true; assert(pocket_button_hold_threshold() == 2000);
 held = 2000; on_button_long_press(); assert(calls == 1 && resets == 1);
 busy = true; on_button_short_press(); assert(!pages);
 busy = false; on_button_short_press(); assert(pages == 1);
 confirm = true; assert(pocket_button_hold_threshold() == 5000);
 held = 5000; on_button_long_press(); assert(calls == 1 && resets == 1);
 on_button_short_press(); assert(!confirms && pages == 1);
 ble = true; generation = 7; on_button_short_press(); assert(confirms == 1 && pages == 1);
 setup = false; on_button_long_press(); assert(resets == 1);
 confirm = false; held = 2000; on_button_long_press(); assert(resets == 1);
 held = 5000; on_button_long_press(); assert(resets == 2);
 on_button_double_press(); assert(calls == 1 && resets == 2 && factory == 5);
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "routes.c"
            binary = Path(directory) / "routes"
            source.write_text(doubles + callbacks + assertions)
            subprocess.run([os.getenv("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                            "-fsanitize=address,undefined", str(source), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
