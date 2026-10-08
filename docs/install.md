# 安装、配对与恢复

只适用于 **Xteink / 阅星瞳 X4 Pro**。先通过 [CrossPoint 官方工具](https://crosspointreader.com) 安装准确的 **CrossPoint 1.6.5 X4 Pro**。更新时保持电量充足。

## 上传个人镜像

1. 按 [构建与打包](build.md) 生成含自己 token 的个人应用镜像。
2. 阅读器打开 **File Transfer → Join Network**，连接电脑可访问的 Wi-Fi。
3. 打开阅读器屏幕显示的实际网址，将个人 `.bin` 上传到 SD 卡根目录。
4. 下载完整文件，核对大小和 SHA256 与本机一致。同名文件未知或内容不一致时不要覆盖，改用新文件名。

代理使用 `/api/status` 确认 `xteink_x4_pro` 和 `1.6.5`，使用 `/api/files?path=/` 检查同名、`POST /upload?path=/` 的 `file` 表单字段上传，再 `GET /download?path=/FILENAME` 完整回读。上传成功不等于刷写成功。

## SD 卡更新

1. 退出 File Transfer。
2. 进入 **Settings → System → SD Card Firmware Update**。
3. 选择已核验的准确个人镜像文件，确认并等待完成，不要断电。
4. Muse Pocket 启动后按右键进入设置，确认显示 **返回 CrossPoint**（英文模式是 Return to CrossPoint）。

如果显示 **恢复未验证**，停止并检查恢复版本与构建，不要绕过校验。首次启动未能通过本地启动检查时，系统可能在五分钟后返回 CrossPoint；正常配对没有五分钟时限。

CrossPoint 的 SD 更新写入非活动应用槽，保留自身在另一个槽；不改 bootloader、分区表、eFuses 或配对存储。仅使用应用镜像，不使用完整刷机包。

## Muse 配对

1. 手机 Muse App 的设备设置中启用 **Developer mode**。
2. 点 **Add gadget**，选择名字带 **MuseGadget…Pocket** 的设备。
3. 阅读社区设备访问说明，确认只配对可信固件。
4. 阅读器出现提示时按左键确认。
5. 手机选择 Wi-Fi，完成连接。

连接后固件会请求 Muse 发送角色图与状态。如果仍是占位图，见 [使用说明](usage.md)。固件更新保留已有配对与 Wi-Fi，正常更新无需重置。

## 返回 CrossPoint

设置中选 **返回 CrossPoint**，长按电源 **3 秒**。开机恢复路径是启动时按住 **右键**至少 **1.2 秒**，它在 Muse 网络和屏幕初始化前执行。

恢复只认可另一个槽内以下准确固件：

- CrossPoint 1.6.5，X4 Pro。
- 大小 `5632640` 字节。
- SHA256 `9ebd6ef1e0bb39ff8dcbff3947f938cb6158a1bcb1769d3811cb8bc6c6667eab`。

更新或重装 Muse Pocket 时，先返回 CrossPoint，再重复 SD 更新。不要通过 Muse Pocket 写别的系统到恢复槽；远程 OTA 已禁用。

这里保留的是 CrossPoint，不是原厂 XTOS。恢复到 XTOS 需遵循厂商或 CrossPoint 官方指导，硬件恢复可能需要磁吸 USB 适配器。CrossMux 与中文 Muse 共存不在本次版本范围。
