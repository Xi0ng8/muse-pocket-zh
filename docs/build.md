# 构建与个人打包

本项目使用 **ESP-IDF 6.0.1**，目标为 **ESP32-S3 / X4 Pro**。支持 macOS、Linux 或 Windows WSL。不要使用普通 X4/X3 固件。

## 构建

先按 [Espressif 官方说明](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32s3/get-started/index.html) 安装 ESP-IDF 6.0.1 和 ESP32-S3 工具。工具链放在仓库外。

```sh
source /path/to/esp-idf/export.sh
idf.py --version
git clone https://github.com/Xi0ng8/muse-pocket-zh.git
cd muse-pocket-zh
esp32/tools/pocket/build.sh
python3 -m unittest discover -s esp32/tools/pocket -p 'test_*.py' -v
```

将 `/path/to/esp-idf` 换成实际目录。版本必须包含 v6.0.1。构建只生成应用镜像，不操作阅读器。首次构建需要下载托管依赖。

输出 `esp32/build-xteink-s3/muse-gadget.bin` **不含 token**，需要本机打包个人副本才能配对。公开预发布中的无凭据应用镜像也使用同一打包流程。

## 写入自己的 token

自行登录 [Muse SDK token 页面](https://gadgets.muse.ai/settings/sdk-tokens)，阅读并接受适用条款，创建 token。在本机终端运行：

```sh
python3 esp32/tools/pocket/package_private.py \
  esp32/build-xteink-s3/muse-gadget.bin \
  artifacts/muse-pocket-zh-private.bin
```

在隐藏提示中粘贴 token，按回车。输入不回显，token 不写入源码、配置、命令参数或构建日志。工具只修改保留字段并重算应用校验和，输出使用仅当前用户可读写的权限；已有文件不会被覆盖，更新时请换新文件名。

自动化本机流程可以使用 `--token-stdin` 从可信管道读取。不要用含明文 token 的 echo 命令，也不要把 token 发到聊天或放进 shell 历史。

输出会显示大小和 SHA256，保留供传输回读核对。**个人镜像含有你的 token，禁止分享或上传 GitHub。** `.gitignore` 只是辅助措施。

## 校验

```sh
python -m esptool --chip esp32s3 image-info artifacts/muse-pocket-zh-private.bin
```

确认 ESP32-S3、checksum 和 validation hash 有效，然后按 [安装指南](install.md) 上传并回读验证。

如果 idf.py 不存在，先激活环境；如果版本不对，使用 6.0.1。请使用 Pocket 的 build.sh，而非 SDK 默认板型。字体数据已随源码提供，普通构建不需安装字体生成依赖，重新生成字库的要求见 `tools/fonts/`。

**不要运行 idf.py flash，不要写 bootloader、分区表或 eFuses。** 本项目通过 CrossPoint 的 SD 卡应用更新安装。
