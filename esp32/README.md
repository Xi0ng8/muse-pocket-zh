<!--
Copyright (c) Meta Platforms, Inc. and affiliates.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

<!-- Modified by Muse Pocket: human documentation for the X4 Pro companion. -->
# X4 Pro companion firmware

This directory contains Muse Pocket's ESP32-S3 firmware and the retained
components of the Muse Gadget SDK. Start with the repository's
[README](../README.md), [build guide](../docs/build.md),
[installation and recovery guide](../docs/install.md), and
[controls and commands](../docs/usage.md).

Build from the repository root:

```sh
esp32/tools/pocket/build.sh
```

Use **ESP-IDF 6.0.1**. The helper selects the X4 Pro profile, creates an
uncredentialed application image, and never flashes a reader. A separate
private-packaging step adds your own Muse SDK token using a hidden prompt.

Install only that private application image through CrossPoint 1.6.5's SD-card
firmware updater. **Do not use `idf.py flash` on the X4 Pro.** Keep the reader's
bootloader, partition table and CrossPoint recovery slot intact.

The original SDK supports other boards and contains tools for them; Muse Pocket
qualifies the X4 Pro path only. For upstream SDK documentation, see
[facebookincubator/muse-gadget-sdk](https://github.com/facebookincubator/muse-gadget-sdk/tree/main/esp32).
For coding agents, read [AGENTS.md](AGENTS.md).
