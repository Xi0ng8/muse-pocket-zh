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

<!-- Modified by Muse Pocket: X4 Pro-specific build and installation guidance. -->
# X4 Pro firmware work

For this repository, follow [the root agent guide](../AGENTS.md) and the
[Muse Pocket skill](../skills/muse-pocket/SKILL.md). The supported product is
**Xteink X4 Pro**, target **esp32s3**.

Build from the repository root with `esp32/tools/pocket/build.sh`. The helper
selects ESP-IDF 6.0.1 and `devices/sdkconfig.xteink-x4-pro`. Add the user's own SDK
token only afterward with the private packager; never put it in `sdkconfig`.

Install through CrossPoint's **Settings → System → SD Card Firmware Update**,
using only the application image. **Never run `idf.py flash` for this reader.**
It would overwrite the bootloader and partition table. Keep the pinned CrossPoint
1.6.5 X4 Pro image in the other slot, and leave remote Muse OTA disabled.

Read [build](../docs/build.md), [install/recovery](../docs/install.md) and
[commands](../docs/usage.md) as needed. The retained SDK tools for other boards
are upstream reference material; they are not X4 Pro flashing instructions.
Preserve third-party licenses. The public placeholder art is original source;
SDK Jollybot assets and generated user characters are not included.
