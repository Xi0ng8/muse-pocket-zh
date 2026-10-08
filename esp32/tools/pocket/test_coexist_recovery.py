"""Compile the real recovery implementation against deterministic ESP/PSA stubs."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

STUBS = r"""
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define ESP_OK 0
#define PSA_SUCCESS 0
#define PSA_ALG_SHA_256 1
#define PSA_HASH_OPERATION_INIT {0}
#define GPIO_NUM_1 1
#define GPIO_NUM_7 7
#define GPIO_MODE_OUTPUT 1
#define GPIO_MODE_INPUT 0
#define GPIO_PULLUP_ONLY 1
#define pdMS_TO_TICKS(n) (n)
#define ESP_LOGW(...) ((void)0)
typedef struct { uint32_t size; } esp_partition_t;
typedef struct { size_t bytes; } psa_hash_operation_t;
int psa_crypto_init(void);
int psa_hash_setup(psa_hash_operation_t*, int);
int psa_hash_update(psa_hash_operation_t*, const void*, size_t);
int psa_hash_finish(psa_hash_operation_t*, uint8_t*, size_t, size_t*);
int psa_hash_abort(psa_hash_operation_t*);
int esp_partition_read(const esp_partition_t*, size_t, void*, size_t);
const esp_partition_t* esp_ota_get_next_update_partition(const void*);
int esp_ota_set_boot_partition(const esp_partition_t*);
void esp_restart(void);
void gpio_hold_dis(int);
void gpio_set_direction(int,int);
void gpio_set_level(int,int);
void gpio_set_pull_mode(int,int);
int gpio_get_level(int);
void vTaskDelay(int);
"""

HARNESS = r"""
#include "stubs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pocket_recovery.c"

static esp_partition_t peer = {6000000};
static const esp_partition_t* candidate = &peer;
static const esp_partition_t* selected;
static unsigned restarts, selects, reads, aborts;
static size_t offset, total, expected_length;
static int image_kind, failure;
static bool changed;

int psa_crypto_init(void) { return failure==1?-1:0; }
int psa_hash_setup(psa_hash_operation_t* op, int alg) {
    assert(alg==PSA_ALG_SHA_256); op->bytes=0; offset=0;
    return failure==2?-1:0;
}
int esp_partition_read(const esp_partition_t* p,size_t pos,void* out,size_t n) {
    assert(p==candidate && pos==offset && n>0 && n<=2048 && pos+n<=p->size);
    ++reads;
    if(failure==3 && pos>=2048) return -1;
    memset(out,changed?0x55:0xaa,n); offset+=n; total+=n; return 0;
}
int psa_hash_update(psa_hash_operation_t* op,const void* bytes,size_t n) {
    assert(bytes); op->bytes+=n; return failure==4?-1:0;
}
int psa_hash_finish(psa_hash_operation_t* op,uint8_t* out,size_t capacity,size_t* n) {
    assert(capacity==32);
    memset(out,0,32);
    // The digest is valid only after hashing the complete simulated image.
    if(!changed && op->bytes==expected_length) {
        memcpy(out,image_kind==1?crossmux_sha:crosspoint_sha,32);
    }
    *n=failure==6?31:32; return failure==5?-1:0;
}
int psa_hash_abort(psa_hash_operation_t* op) { (void)op; ++aborts; return 0; }
const esp_partition_t* esp_ota_get_next_update_partition(const void* p) { assert(!p); return candidate; }
int esp_ota_set_boot_partition(const esp_partition_t* p) {
    ++selects; selected=p; return failure==7?-1:0;
}
void esp_restart(void) { ++restarts; }
void gpio_hold_dis(int p) { (void)p; }
void gpio_set_direction(int p,int mode) { (void)p; (void)mode; }
void gpio_set_level(int p,int level) { (void)p; (void)level; }
void gpio_set_pull_mode(int p,int mode) { (void)p; (void)mode; }
int gpio_get_level(int p) { (void)p; return 1; }
void vTaskDelay(int n) { (void)n; }

static void reset(int kind) {
    s_checked=false; s_recovery=NULL; s_crossmux=false;
    candidate=&peer; peer.size=6000000; selected=NULL;
    restarts=selects=reads=aborts=0; offset=total=0; changed=false; failure=0;
    image_kind=kind; expected_length=kind==1?crossmux_bytes:crosspoint_bytes;
}
static void rejected(void) {
    assert(!pocket_return_to_crosspoint()); assert(!restarts && !selects);
}
int main(int argc,char** argv) {
    assert(argc==2); reset(1);
    if(!strcmp(argv[1],"crossmux")) {
        assert(pocket_recovery_available() && pocket_recovery_is_crossmux());
        assert(total==crossmux_bytes && reads==3 && offset==crossmux_bytes);
        unsigned old_reads=reads; assert(pocket_recovery_available() && reads==old_reads);
        assert(pocket_return_to_crosspoint());
        assert(selected==candidate && selects==1 && restarts==1 && reads==old_reads*2);
    } else if(!strcmp(argv[1],"crosspoint")) {
        reset(2); assert(pocket_recovery_available() && !pocket_recovery_is_crossmux());
        assert(total==crossmux_bytes+crosspoint_bytes && offset==crosspoint_bytes);
        assert(pocket_return_to_crosspoint());
        assert(selected==candidate && selects==1 && restarts==1);
    } else if(!strcmp(argv[1],"changed")) {
        assert(pocket_recovery_available()); unsigned old_reads=reads;
        changed=true; rejected(); assert(reads>old_reads && !s_recovery);
    } else if(!strcmp(argv[1],"bad_hash")) {
        changed=true; rejected(); assert(reads && aborts);
    } else if(!strcmp(argv[1],"bad_size")) {
        peer.size=crossmux_bytes-1; rejected(); assert(!reads && !aborts);
        assert(!matches_pin(&peer,0,crossmux_sha));
        assert(!matches_pin(NULL,crossmux_bytes,crossmux_sha));
    } else if(!strcmp(argv[1],"no_partition")) {
        candidate=NULL; rejected(); assert(!reads);
    } else if(!strcmp(argv[1],"prefix")) {
        expected_length=2048; rejected(); assert(offset==crosspoint_bytes);
    } else if(!strcmp(argv[1],"boot_failure")) {
        failure=7; assert(!pocket_return_to_crosspoint());
        assert(selects==1 && selected==candidate && !restarts);
    } else {
        assert(!strncmp(argv[1],"failure",7)); failure=argv[1][7]-'0';
        assert(failure>=1 && failure<=6); rejected();
        if(failure==3) assert(offset==2048);
    }
    return 0;
}
"""


class CoexistRecoveryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.directory = Path(cls.temp.name)
        (cls.directory / "stubs.h").write_text(STUBS)
        for name in ("driver/gpio.h", "esp_log.h", "esp_ota_ops.h", "esp_partition.h",
                     "esp_system.h", "psa/crypto.h", "freertos/FreeRTOS.h", "freertos/task.h"):
            path = cls.directory / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('#include "stubs.h"\n')
        for name in ("pocket_recovery.c", "pocket.h"):
            (cls.directory / name).write_bytes((ROOT / "main" / name).read_bytes())
        # Override only the CrossMux pin in this temporary copy; production pins stay intact.
        (cls.directory / "pocket_crossmux_pin.h").write_text(
            '#include <stdint.h>\nstatic const uint32_t crossmux_bytes=4099;\n'
            'static const uint8_t crossmux_sha[32]={0x42};\n')
        (cls.directory / "harness.c").write_text(HARNESS)
        cls.binary = cls.directory / "recovery_harness"
        subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", "-g", "-I", str(cls.directory),
                        str(cls.directory / "harness.c"), "-o", str(cls.binary)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def test_actual_recovery_paths(self):
        for scenario in ("crossmux", "crosspoint", "changed", "bad_hash", "bad_size",
                         "no_partition", "prefix", "boot_failure",
                         *(f"failure{i}" for i in range(1, 7))):
            with self.subTest(scenario=scenario):
                subprocess.run([str(self.binary), scenario], check=True)


if __name__ == "__main__":
    unittest.main()
