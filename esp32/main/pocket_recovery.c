// Local recovery trusts exact pinned X4 Pro images, never a firmware name.
#include "pocket.h"
#include "pocket_crossmux_pin.h"
#include <string.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "psa/crypto.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const uint32_t crosspoint_bytes = 5632640;
static const uint8_t crosspoint_sha[32] = {
    0x9e,0xbd,0x6e,0xf1,0xe0,0xbb,0x39,0xff,0x8d,0xcb,0xff,0x39,0x47,0xf9,0x38,0xcb,
    0x61,0x58,0xa1,0xbc,0xb1,0x76,0x9d,0x38,0x11,0xcb,0x8b,0xc6,0xc6,0x66,0x7e,0xab
};
static const esp_partition_t* s_recovery;
static bool s_checked;
static bool s_crossmux;

static bool matches_pin(const esp_partition_t* candidate, uint32_t image_bytes,
                        const uint8_t expected[32]) {
    if (!candidate || !image_bytes || candidate->size < image_bytes) return false;
    // Hash the original file length, including its board tag; generic Arduino
    // app descriptors do not identify CrossPoint or the correct physical board.
    if(psa_crypto_init()!=PSA_SUCCESS)return false;
    psa_hash_operation_t sha = PSA_HASH_OPERATION_INIT;
    uint8_t buffer[2048], digest[32];
    int result = psa_hash_setup(&sha, PSA_ALG_SHA_256);
    for (uint32_t pos=0; !result && pos<image_bytes; pos+=sizeof(buffer)) {
        uint32_t n = image_bytes-pos;
        if (n>sizeof(buffer)) n=sizeof(buffer);
        if (esp_partition_read(candidate,pos,buffer,n) != ESP_OK) { result=-1; break; }
        result = psa_hash_update(&sha,buffer,n);
    }
    size_t digest_size=0;
    if (!result) result=psa_hash_finish(&sha,digest,sizeof(digest),&digest_size);
    psa_hash_abort(&sha);
    return !result && digest_size == sizeof(digest) &&
        memcmp(digest,expected,sizeof(digest)) == 0;
}
bool pocket_recovery_available(void) {
    if (s_checked) return s_recovery != NULL;
    s_checked = true;
    const esp_partition_t* candidate = esp_ota_get_next_update_partition(NULL);
    s_crossmux=matches_pin(candidate,crossmux_bytes,crossmux_sha);
    if (!s_crossmux && !matches_pin(candidate,crosspoint_bytes,crosspoint_sha)) return false;
    s_recovery=candidate;
    return true;
}
bool pocket_recovery_is_crossmux(void) {
    return pocket_recovery_available() && s_crossmux;
}
bool pocket_return_to_crosspoint(void) {
    // Re-read the full pin before changing boot state, even after an earlier
    // health/menu check; unexpected peer writes must not reuse cached trust.
    s_checked=false;
    s_recovery=NULL;
    if (!pocket_recovery_available()) return false;
    if (esp_ota_set_boot_partition(s_recovery) != ESP_OK) return false;
    esp_restart();
    return true;
}
void pocket_boot_guard(void) {
    // The reader's peripheral latch must remain on, including after sleep.
    gpio_hold_dis(GPIO_NUM_1);
    gpio_set_direction(GPIO_NUM_1,GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_1,1);
    gpio_set_direction(GPIO_NUM_7,GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_7,GPIO_PULLUP_ONLY);
    // Holding RIGHT while starting bypasses all Muse display/storage/network code.
    if (gpio_get_level(GPIO_NUM_7)==0) {
        vTaskDelay(pdMS_TO_TICKS(1200));
        if (gpio_get_level(GPIO_NUM_7)==0 && !pocket_return_to_crosspoint())
            ESP_LOGW("link.pocket.recovery","verified reader slot unavailable");
    }
}
