#!/usr/bin/env bash
# Build an uncredentialed, app-only ESP32-S3 image with ESP-IDF 6.0.1.
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
pocket_root="$(cd -- "$script_dir/../../.." && pwd)"
if ! command -v idf.py >/dev/null 2>&1; then
    if [[ -n "${IDF_PATH:-}" && -f "$IDF_PATH/export.sh" ]]; then
        # ESP-IDF's environment script handles its own optional variables.
        set +u
        source "$IDF_PATH/export.sh"
        set -u
    else
        echo "Activate ESP-IDF 6.0.1 first: source /path/to/esp-idf/export.sh" >&2
        exit 1
    fi
fi
if [[ "$(idf.py --version)" != *"v6.0.1"* ]]; then
    echo "Muse Pocket requires ESP-IDF 6.0.1." >&2
    exit 1
fi
cd "$pocket_root/esp32"
idf.py -B build-xteink-s3 -D SDKCONFIG=build-xteink-s3/sdkconfig \
    -D IDF_TARGET=esp32s3 -D PROJECT_VER=0.1.3-pocket \
    -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;devices/sdkconfig.xteink-x4-pro' build
