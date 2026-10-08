# CrossMux 与中文 Muse 双系统

**`v0.1.5-coexist-zh` 是实验预发布，只适用于阅星瞳 / Xteink X4 Pro（ESP32-S3）。** 配套 CrossMux 为 `1.6.5-x4pro-coexist`，使用独立 `x4pro_coexist` 配置。最终两个应用槽分别运行 CrossMux 和中文 Muse Pocket；CrossMux 替代 CrossPoint，不存在第三个常驻系统。

原 `v0.1.4-zh` 用于 CrossPoint 1.6.5 X4 Pro 环境，继续见 [旧版安装指南](install.md)。它不是双系统版，不能替代本次配套 Muse。

## 修改了什么

- CrossMux 的系统设置增加 **切换到中文 Muse**，完整验证另一槽的固件与配套证书后选择下次启动应用。
- Muse 菜单识别配套 CrossMux，显示 **切换到 CrossMux**；切换前重新读取完整镜像，核对准确长度和 SHA256。旧 CrossPoint 的精确摘要保留，用于旧环境与迁移阶段。
- CrossMux 禁用把另一应用槽当作字体 Flash 缓存，保护 Muse 镜像。SD 字体与原有 SD/PSRAM 字体路径仍可使用，读取字体可能更依赖 SD 卡速度和可用内存。
- CrossMux 禁用普通 OTA；SD 更新只接受与当前 CrossMux 配套的已签署 Muse。这项限制同样适用于开机 SD 恢复模式。
- CrossMux 的 NVS 初始化错误不再自动清空整区，以免删除 Muse 配对信息。遇到这类错误应诊断与恢复，不能把它理解为保证所有已有配对都一定可用。

阅读器、中文排版、书库和硬件 SDK 继续使用 CrossMux 现有实现；Muse 的中文显示、角色图和状态命令继续使用本项目实现。

## 安装步骤

安装前备份 SD 卡书籍、进度和重要文件，保持电量充足。更新中不拔卡、不重启、不切断电源。准备可用的 USB 数据适配器并阅读 [恢复与风险应对](crossmux-recovery.md)，但不要把拥有适配器理解为已经验证 USB 恢复可行。

### 准备：核验公开文件并生成个人 Muse

从 [本版本发布页](https://github.com/Xi0ng8/muse-pocket-zh/releases/tag/v0.1.5-coexist-zh) 下载全部附件，以便核对 `SHA256SUMS`；不从普通 CrossMux Nightly 下载替代镜像。主要文件包括：

- `crossmux-x4pro-coexist-1.6.5.bin`：专用 CrossMux 应用镜像。
- `muse-pocket-zh-0.1.5-coexist-uncredentialed.bin`：已签署、尚未加入 SDK token 的公开 Muse 模板。
- `package_private.py`：本机个人打包工具。
- `SHA256SUMS` 与 `verification.json`：公开文件核对与验证记录。
- `coexistence.patch` 与 `CrossMux-LICENSE.txt`：配套源码修改和许可证。

在 macOS 本机核对下载内容：

```sh
shasum -a 256 -c SHA256SUMS
```

Linux 可使用 `sha256sum -c SHA256SUMS`。准确字节数与 SHA256 也记录在 [发布清单](../integrations/crossmux/release.json)，以清单为准，不根据文件名推断兼容性。

自行登录 [Muse SDK token 页面](https://gadgets.muse.ai/settings/sdk-tokens)，阅读适用条款并创建自己的 token。在本机下载目录运行：

```sh
python3 package_private.py \
  muse-pocket-zh-0.1.5-coexist-uncredentialed.bin \
  muse-pocket-zh-0.1.5-coexist-private.bin
```

在隐藏提示中粘贴 token。工具不回显 token，不覆盖已有输出文件；保存其输出的个人文件大小和 SHA256。公开模板的配套证书已经签署，本地加入 SDK token 不影响该证书验证；用户不需要签名私钥。

**个人 `.bin` 含有 SDK token，不能上传 GitHub、分享或附到问题报告。** 不把 token 放入命令参数、聊天、shell 历史或构建配置。源码使用相同工具 `esp32/tools/pocket/package_private.py`；不要用自行普通构建、未签署的 Muse 替代公开模板。

### 阶段 A：从原 CrossPoint 安装专用 CrossMux

1. 确认当前阅读器是 **CrossPoint 1.6.5 X4 Pro**。如果目前运行旧 Muse，先按 [旧版恢复步骤](install.md#返回-crosspoint) 返回它。
2. 使用 CrossPoint 的 **File Transfer → Join Network**，打开阅读器实际显示的网址，把 `crossmux-x4pro-coexist-1.6.5.bin` 上传至 SD 根目录。也可通过读卡器复制到 SD。
3. 从阅读器完整下载回读该文件，确认大小与 SHA256 和本机下载文件一致。同名内容未知时使用新文件名，不覆盖未知文件。
4. 退出文件传输，进入 **Settings → System → SD Card Firmware Update**，选择核验过的专用 CrossMux 文件，等待完成。
5. 确认 CrossMux 启动、屏幕与按键正常、SD 卡可读后，再进行阶段 B。此时另一个槽仍是 CrossPoint；不要把这一步当作已经安装了 Muse。

### 阶段 B：从 CrossMux 安装配套个人 Muse

1. 将本机生成的 `muse-pocket-zh-0.1.5-coexist-private.bin` 复制或通过阅读器文件传输上传到 SD 根目录。网络传输后完整回读，核对本机个人文件的大小与 SHA256。
2. 在 CrossMux 的 **设置 → 系统 → SD 卡固件更新**（英文 **Settings → System → SD Card Firmware Update**）选择该个人 Muse 文件，确认并等待完成。签名或配套校验失败时停止，不绕过检查。
3. Muse 启动后按右键打开设置，确认显示 **切换到 CrossMux**。显示 **恢复未验证** 时不要继续切换或升级，按 [故障处理](crossmux-recovery.md) 检查。
4. 首次配对时在手机 Muse App 启用 Developer mode，通过 Add gadget 选择 MuseGadget…Pocket，阅读访问说明，在阅读器上按左键确认并选择 Wi-Fi。已有配对先检查能否重连，不先重置。
5. 核对两个方向的切换，并检查自己的 Muse 配对、中文阅读和字体使用。阶段 B 替换最后的 CrossPoint 槽，最终只剩 CrossMux 与 Muse。

## 日常切换与升级

CrossMux：打开 **设置 → 系统 → 切换到中文 Muse**，选择该项。

Muse：按右键打开设置，用右键选择 **切换到 CrossMux**，长按电源 **3 秒**。短按此项不会切换；长按电源在其他项上会进入休眠。英文模式显示 **Switch to CrossMux**。

两边镜像相互绑定。后续升级必须使用重新构建、固定摘要与签署的配套发布；不要直接安装普通 CrossMux Stable/Nightly、普通 Muse 或其他板型镜像。专用 CrossMux 的 SD 更新只接受配套签署 Muse，不能通过该入口更新 CrossMux 自身。

## 验证范围

2026-10-08，用户在一台国内版 X4 Pro 上确认专用 CrossMux 启动和 CrossMux ↔ 中文 Muse 双向切换正常。原 `0.1.4-zh` 的中文显示也曾得到用户实机确认。

主机测试、构建、上传回读及准确公开资产记录见 [发布清单](../integrations/crossmux/release.json) 和发布页 `verification.json`。**`0.1.5-coexist-zh` 的 Muse 配对同步、中文阅读、换字体后的实际 Flash 槽摘要、断电故障恢复及 USB 恢复尚未单独确认。** 主机字体回退或恢复测试不能代替这些实机证据，也不代表所有硬件批次已验证。

## 来源与许可证

Muse 来源于 [viticci/muse-pocket](https://github.com/viticci/muse-pocket)，保留 [Apache-2.0](../LICENSE) 与 [NOTICE](../NOTICE)。中文点阵源自 Noto Sans CJK SC，按 [SIL OFL 1.1](../esp32/main/fonts/OFL.txt) 分发。

CrossMux 来源于 [0x1abin/crossmux](https://github.com/0x1abin/crossmux)，本次上游版本、修改补丁与构建入口见 [配套源码说明](../integrations/crossmux/README.md)。保留 [CrossMux 上游 MIT 许可证](../integrations/crossmux/LICENSE.upstream)，发布页同时附 `CrossMux-LICENSE.txt` 和 `coexistence.patch`。依赖各自保留其许可证；社区修改不改变上游授权。
