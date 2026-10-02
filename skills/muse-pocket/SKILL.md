---
name: muse-pocket
description: Build, maintain or troubleshoot Muse Pocket companion firmware on an Xteink X4 Pro, including private token packaging, paired character/status commands and CrossPoint SD updates.
---

# Muse Pocket

Use this skill in a Muse Pocket checkout. It is for the **X4 Pro**, not the X4,
X3 or a generic ESP32 board. Read the repository's root `AGENTS.md` first.

## Build or change the firmware

Read `docs/build.md` for toolchain and packaging steps, and `CONTRIBUTING.md` for
verification. Use ESP-IDF **6.0.1** and `esp32/tools/pocket/build.sh`; the default
SDK build targets a different chip. Run the focused Pocket tests, relevant SDK
host tests when shared code changes, and the public-tree check before publishing.
Preserve upstream license headers and the recovery digest check.

The default build has no token. The separate private packager reads the user's
own token through a hidden prompt or trusted stdin, then patches a private image.
Do not ask users to paste credentials into chat, put them in shell arguments,
print them, or publish the resulting firmware. Do not assume a password manager,
computer address, Wi-Fi network, paired Muse name or remote-control tool.

## Install or recover

Read `docs/install.md` and the device-operation section of `AGENTS.md`. Establish
the actual reader model, CrossPoint version and URL shown by its File Transfer
screen. Upload only the private application image and verify a full download
readback by size and SHA256. Reuse an exact matching file; stop on an unknown
existing file instead of overwriting it. Upload success is not flash success.

Use CrossPoint's **SD Card Firmware Update** into the inactive slot. Never run
`idf.py flash` or write a bootloader, partition table, merged image or eFuses on
this reader. Keep the pinned CrossPoint 1.6.5 X4 Pro image in the other slot.
Firmware update selection and pairing confirmation require physical button taps;
request those when remote access cannot perform them.

Verify the recovery row, character, caption and settings on the device after an
authorized update. Report what is compiled, transferred, physically observed or
confirmed by the user. The held-Right startup return works before Muse setup but
still requires the pinned recovery image. Do not weaken its check or enable
remote OTA to bypass an update problem.

## Character or status problems

Read `docs/usage.md`. A connected transport does not prove Muse sent an image or
caption. Check the paired identity, the Muse's command results and the image
format. Use `display.draw_url` for a 480×480 baseline JPEG and `pocket.set_status`
for short ASCII text; a previous image must survive a failed download. The startup
chat request asks Muse to keep the status current, but it is not an independent
activity subscription. Do not reset pairing as a first response to a UI problem.
