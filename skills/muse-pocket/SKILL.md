---
name: muse-pocket
description: Build, maintain or troubleshoot Muse Pocket on an Xteink X4 Pro, including private packaging, protected CrossMux coexistence, character/status commands and experimental left-button Muse calls.
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

Use the installed reader's **SD Card Firmware Update** into the inactive slot; the dedicated CrossMux updater accepts only its signed paired Muse image. Never run
`idf.py flash` or write a bootloader, partition table, merged image or eFuses on
this reader. Use the pinned CrossPoint peer only for the legacy version. For CrossMux coexistence, follow docs/crossmux-coexistence.md and docs/crossmux-recovery.md: preserve the exact protected CrossMux peer, use the signed paired Muse template, and keep both systems bound to their complete image checks.
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
for short Chinese or ASCII text; a previous image must survive a failed download. The startup
chat request asks Muse to keep the status current, but it is not an independent
activity subscription. Do not reset pairing as a first response to a UI problem.

## Experimental left-button call

Read `docs/call-muse.md` before working on `0.1.6-call-zh`. This is source under
development, not a published or hardware-verified release. Preserve the existing
protected CrossMux 1.6.5 peer; upgrade only a newly paired signed Muse template
with local private packaging. Do not rewrite the 0.1.5 release or its hashes.

The paired device's 2-second left hold sends a fixed trigger with fresh
`call_id` and `device_id`; it does not guess a recent task. Triggering from a
paired settings menu closes that menu. Preserve a 5-second setup reset only
while initial setup is incomplete and no pairing confirmation is pending;
daily paired use must not clear pairing. During pairing confirmation a hold
must neither confirm nor call; confirmation requires a short press. Unpaired
devices must not call. Busy repeats must not resend. HTTP 2xx is acceptance, not task
completion. Only `pocket.complete_call(call_id,text)` with the exact pending ID
and 1–3072 UTF-8 bytes completes the call. Ordinary `pocket.set_status` must not
override the calling screen. Results page through the avatar area, preserving
the avatar; a short left press advances and returns to the avatar after the last
page. Preserve right/settings, power/sleep and recovery controls.

After three minutes without a matched result, show unknown status without an
automatic retry. Disconnect, sleep or leaving a local page does not cancel an
already sent Muse task. Check the Muse task before retrying; side effects remain
under Muse's original user authorization and confirmation requirements.

Help the user save a named “X4 Pro 左键预设” through their Muse chat as persistent
Memory, then read it back and dry-run it in a new message. Use the concrete
template in `docs/call-muse.md`, including exact callback context and a read-only
summary example. Missing, forgotten or ambiguous presets must return a setup
hint; never execute a guessed recent request. Do not invent a preset API, cloud
scheduler, phone menu or Memory path. This repository skill is maintenance
guidance, not proof that it is installed in the user's Muse or that their preset
has been saved. Distinguish source tests from cloud setup and physical validation.
