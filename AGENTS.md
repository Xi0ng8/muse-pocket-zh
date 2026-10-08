# Working on Muse Pocket

Muse Pocket is firmware for the **Xteink X4 Pro's ESP32-S3**. Its e-paper screen
shows the user's paired Muse, a character image and a short activity caption.
This repository retains SDK components and tools for other boards, but the
supported product here is the X4 Pro. Use the Pocket build and SD-update route.

Read [README.md](README.md), then the guide relevant to the task:

- [Build and private packaging](docs/build.md).
- [Installation and recovery](docs/install.md).
- [Commands and behavior](docs/usage.md).
- [Contributing and verification](CONTRIBUTING.md).
- [Reusable agent skill](skills/muse-pocket/SKILL.md).
- [CrossMux coexistence](docs/crossmux-coexistence.md) and [recovery](docs/crossmux-recovery.md).

## Source map

| Path | Purpose |
| --- | --- |
| `esp32/main/pocket_status.cpp` | Portrait UI, caption wrapping, settings and buttons |
| `esp32/main/pocket_recovery.c` | Check and boot exact pinned CrossPoint or protected CrossMux images |
| `esp32/main/pocket_token.c` | Reserved token field patched only in private output |
| `esp32/main/app.c` | Startup, boot confirmation and custom command handlers |
| `esp32/main/noise_control.cpp` | Paired Muse protocol, command registration, identity and setup request |
| `esp32/main/image_fetch.c` | Image download and staged completion |
| `esp32/components/xteink_epd` | Native ESP-IDF compatibility and FreeInk panel drivers |
| `esp32/devices/sdkconfig.xteink-x4-pro` | X4 Pro build defaults |
| `esp32/tools/pocket` | Build helper, private packager and focused tests |

## Boundaries that matter

- Build with **ESP-IDF 6.0.1**, target **esp32s3**, and the X4 Pro overlay. The
  SDK's default board is different. Other upstream board helpers are not the
  X4 Pro installation path.
- Do not put SDK or paired-device tokens, Wi-Fi passwords, user names or network
  addresses into source, docs, logs or command arguments. Let the user enter
  their own SDK token through the packager's hidden prompt or trusted stdin.
- Packaged `.bin` files contain credentials. Keep them local. Source and normal
  builds stay uncredentialed; do not publish generated config or build folders.
- **Never use `idf.py flash`, a merged image or full-flash arguments on this
  reader.** Use the current reader's app-only SD updater; the dedicated CrossMux profile accepts only signed paired Muse images. Leave the bootloader, partition
  table, eFuses, stock calibration and pairing storage alone.
- Legacy 0.1.4 uses exact CrossPoint 1.6.5 X4 Pro recovery. The 0.1.5 coexistence
  build uses the exact protected CrossMux image pinned in pocket_crossmux_pin.h;
  the two installed systems are CrossMux and Muse. Boot confirmation requires
  controls, a completed panel refresh and an exact pinned peer digest. Do not weaken the check or confirm boot merely because Muse
  connected. Remote Muse OTA stays disabled.
- The renderer owns the panel bus. Finish or fail an image transfer without
  corrupting the previous displayed character; wait for a completed frame before
  claiming success or sleeping.
- Do not reset pairing to fix a display or caption problem. Establish the actual
  connection, image-fetch and command result first.

## Device operations

Before transferring, confirm the user's actual X4 Pro and the URL displayed by
its File Transfer screen. `GET /api/status` should identify `xteink_x4_pro` and
the firmware corresponding to the requested installation stage. Do not assume an IP, hostname, SSID, computer or account.

CrossPoint's APIs for this version are:

- `GET /api/files?path=/`: check whether the exact filename already exists.
- `POST /upload?path=/`: multipart form with the private image as field `file`.
- `GET /download?path=/FILENAME`: read the uploaded file back in full.

Compare the readback's length and SHA256 with the local private image. Do not
print binary contents or unrelated SD filenames. Reuse an existing exact match;
stop on a different existing file rather than overwriting it. The reader's SD
update selection requires a physical action; an upload is not a successful flash.

After an authorized update, check the physical recovery row, paired name,
character, caption and settings. Distinguish a successful build, a verified
transfer, a user-confirmed device result and a separately observed recovery test.
Do not claim the panel variant or physical behavior from compilation alone.

## Verification

Run `esp32/tools/pocket/build.sh` and the focused tests described in
[CONTRIBUTING.md](CONTRIBUTING.md). Changes to shared SDK protocol or display
code also need relevant SDK host tests. `tools/check_public_tree.py` scans tracked
files and Git history for credential and artifact hazards without printing
matches. Review its result before publishing, along with `git status` and the
actual staged file list. Passing this check does not make a private binary public.

Preserve upstream license headers and the modification notices. Do not add the
SDK's license-excluded Jollybot art or generated user avatars to this repository.
