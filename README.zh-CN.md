# Muse Pocket 中文版

基于 [Federico Viticci 的 Muse Pocket](https://github.com/viticci/muse-pocket)

把 **阅星瞳 / Xteink X4 Pro** 变成 Muse 随身墨水屏，显示角色图、名字和活动状态。这个社区分支为原项目增加中文显示，不是 Xteink、Meta 或原作者的官方版本。

## 中文支持

- 中文与英文混排的 Muse 名字和状态。
- 按屏幕宽度自动换行，不把 UTF-8 字符截断。
- 默认简体中文菜单、配对与连接提示；名字和状态可以中英混排。
- 内置中文字库，不需要先向 SD 卡安装字体。字体覆盖和限制见 [中文说明](docs/chinese.md)。
- 保留原项目 CrossPoint 1.6.5 X4 Pro 的精确恢复校验。

**当前为预发布开发版本。构建与自动化测试不能证明中文界面已经在所有屏幕上通过实机验证。** 详见 [验证记录](docs/chinese.md#验证)。

## 界面预览

以下由实际渲染代码在电脑上生成，使用示例名字与占位角色，并非实机照片。

![中文名字与混排状态](docs/images/chinese-main-preview.png)

![中文设置菜单](docs/images/chinese-menu-preview.png)

## 适用设备

只支持 **X4 Pro / ESP32-S3**，不支持普通 X4、X3。先安装官方 **CrossPoint 1.6.5 X4 Pro**，再使用它的 **SD Card Firmware Update** 安装 Muse Pocket 的应用镜像。

不要用完整刷机包、改分区或把 CrossMux 覆盖到恢复槽。当前中文分支继续依赖准确版本的 CrossPoint；CrossMux 共存不在本次发布范围。

## 安装

1. 准备 Muse 手机 App，启用 Developer mode，自行创建 [Muse SDK token](https://gadgets.muse.ai/settings/sdk-tokens)。
2. 按 [构建说明](docs/build.md) 使用 ESP-IDF 6.0.1 构建，或从本项目预发布下载无凭据应用镜像。
3. 在本机终端运行打包工具。token 在隐藏提示中输入，不出现在命令参数里：

   ```sh
   python3 esp32/tools/pocket/package_private.py muse-pocket-uncredentialed.bin muse-pocket-private.bin
   ```

4. 阅读器进入 File Transfer → Join Network，上传个人镜像到 SD 卡，下载回读比对大小和 SHA256。
5. 退出文件传输，到 Settings → System → SD Card Firmware Update 选择个人镜像。
6. Muse Pocket 启动后，右键打开设置，确认显示 **返回 CrossPoint**。如果显示 **恢复未验证**，停止配对并检查恢复固件，不要绕过校验。
7. 在手机 Muse App 添加 MuseGadget…Pocket，阅读器左键确认，选 Wi-Fi 完成配对。
8. 主界面与菜单默认中文；首次联网后等待 Muse 发送角色图和状态。

**个人打包镜像包含你的 token，不能上传 GitHub、分享给别人或附到问题报告。** 发布固件始终不包含 token，每个人必须本机打包自己的副本。

完整按键、休眠和恢复步骤见 [安装指南](docs/install.md) 与 [使用指南](docs/usage.md)。

## 让 Muse 发送中文状态

> 请在 Muse Pocket 上显示你的角色图，使用 480×480 baseline JPEG 调用 display.draw_url。调用 pocket.set_status 发送简短中文状态，例如「正在整理笔记」。请保持状态随实际活动更新，并说明任何命令失败。

这是给 Muse 的请求，固件不会独立订阅或推断 Muse 的活动。状态最多接收 240 个 UTF-8 字节、显示四行；不支持彩色 emoji。

## 更新与恢复

保留 CrossPoint 恢复槽。更新中文 Muse Pocket 前先通过 **返回 CrossPoint** 返回 CrossPoint，再使用 SD 卡应用更新流程。不要在 Muse Pocket 中安装别的系统覆盖恢复槽。

## 许可证与贡献

保留上游 [Apache-2.0](LICENSE) 和 [依赖声明](NOTICE)。内置 CJK 字形按其原字体的 SIL Open Font License 1.1 分发，具体许可证见字体目录。欢迎提交中文排版与显示问题；报告不要包含凭据或个人安装包。
