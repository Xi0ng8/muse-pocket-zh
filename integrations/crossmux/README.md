# CrossMux 最小双系统集成

这是现有 Muse Pocket 中文项目附带的补丁，不是独立维护的 CrossMux 分支。使用与风险说明见 [双系统安装](../../docs/crossmux-coexistence.md) 和 [故障应对](../../docs/crossmux-recovery.md)。普通用户优先使用 Release 中已经配套的应用镜像，无需编译或持有签名私钥。

## 来源与改动

- 上游：[0x1abin/crossmux](https://github.com/0x1abin/crossmux)，固定提交 `1512e8e049f2b1a4efc7c004a5b1a93cbc5e3f9f`。
- FreeInk SDK gitlink：`96de1be6ce08eb732909e6e8149af8f892b9a2c5`，未纳入 SDK 修改。
- CrossMux 原许可证为 MIT，随补丁提供 [LICENSE.upstream](LICENSE.upstream)。其依赖仍遵循各自许可证，构建者应保留依赖中的版权与许可文件。
- [upstream.json](upstream.json) 列出补丁 SHA256 和全部修改文件摘要；[release.json](release.json) 列出公开镜像摘要及验证边界。

专用 `x4pro_coexist` 配置增加一个 Muse 切换菜单，使用新增 HAL 验证器校验签名、完整规范化镜像摘要与配套 CrossMux 摘要，再调用正常 SDK 启动选择接口。它禁止另一槽的字体 Flash scratch 擦写，禁用普通 OTA，限制 SD 更新为配套 Muse，并拒绝整区 NVS 自动清空。字体仍走原有 SD/PSRAM 路径。阅读器、排版、书库和硬件 SDK 不作功能修改；其他构建配置不启用共存保护。

保留上游已有的兼容性包装，包括 eFuse block revision 检查 shim；“正常 SDK 选槽”不等于重写或验证了设备 bootloader。设备 bootloader、分区表及 eFuses 一律保留，不保证断电自动回滚。

## 应用补丁与测试

在本仓库根目录执行，目标必须是干净的固定版本 checkout：

```sh
git clone --no-checkout https://github.com/0x1abin/crossmux.git private/crossmux-source
git -C private/crossmux-source checkout --detach 1512e8e049f2b1a4efc7c004a5b1a93cbc5e3f9f
python3 integrations/crossmux/verify_patch.py private/crossmux-source --apply
git -C private/crossmux-source submodule update --init --depth 1 freeink-sdk
python3 -m unittest discover -s private/crossmux-source/scripts/tests -p 'test_muse_coexist*.py' -v
```

主机测试需要 C++ 编译器和 OpenSSL 开发头文件/库。主机密码学测试使用 OpenSSL 适配器；实际 mbedTLS 链接由 ESP 固件构建验证，实机切换另行验证。补丁验证工具拒绝覆盖已有修改。

## 构建自己的配套固件

编译结果可能因为工具链、构建时间等不同而改变，不能假定与发布镜像 SHA256 相同。必须依次重新生成公钥、构建 CrossMux、固定其准确摘要、构建 Muse、签署 Muse；不能只重编其中一边。

工具环境：ESP-IDF **6.0.1**（Muse）、Python **3.12**、`pioarduino==6.2.0`（CrossMux）、`cryptography==46.0.6`（本地证书工具）。CrossMux 平台与依赖由固定上游配置选择。签名私钥只留本机，仓库 `private/` 已忽略。

```sh
python3 -m venv .venv
.venv/bin/python -m pip install pioarduino==6.2.0 cryptography==46.0.6
```

先在本仓库根目录生成自己的 P-256 密钥并更新补丁源码中的公钥：

```sh
.venv/bin/python - <<'PY'
from pathlib import Path
import os
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec
key = ec.generate_private_key(ec.SECP256R1())
path = Path('private/coexist-signing.pem')
path.parent.mkdir(parents=True, exist_ok=True)
fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
with os.fdopen(fd, 'wb') as out:
    out.write(key.private_bytes(serialization.Encoding.PEM,
                                serialization.PrivateFormat.PKCS8,
                                serialization.NoEncryption()))
public = key.public_key().public_bytes(serialization.Encoding.PEM,
                                      serialization.PublicFormat.SubjectPublicKeyInfo).decode()
Path('private/crossmux-source/lib/hal/MuseCoexistPublicKey.h').write_text(
    '#pragma once\ninline constexpr char kMuseCoexistPublicKeyPem[]=R"KEY(' + public + ')KEY";\n')
PY
```

构建 CrossMux，只取 `firmware.bin`：

```sh
task_root="$PWD"
export PLATFORMIO_CORE_DIR="$task_root/private/.platformio"
cd private/crossmux-source
"$task_root/.venv/bin/python" scripts/patch_pioarduino_cache.py --prepare-platform
"$task_root/.venv/bin/pio" run -e x4pro_coexist
cd "$task_root"
mkdir -p artifacts
cp private/crossmux-source/.pio/build/x4pro_coexist/firmware.bin artifacts/crossmux-custom.bin
```

**不要安装 `firmware.factory.bin`，不要执行 upload/full-flash。** 将生成镜像准确绑定到 Muse：

```sh
.venv/bin/python - <<'PY'
from pathlib import Path
import hashlib, sys
sys.path.insert(0, 'esp32/tools/pocket')
from package_private import image_layout
data = Path('artifacts/crossmux-custom.bin').read_bytes()
image_layout(data)
values = ','.join('0x%02x' % x for x in hashlib.sha256(data).digest())
Path('esp32/main/pocket_crossmux_pin.h').write_text(
    '#pragma once\n#include <stdint.h>\n'
    + 'static const uint32_t crossmux_bytes = %d;\n' % len(data)
    + 'static const uint8_t crossmux_sha[32] = {' + values + '};\n')
PY
```

激活 ESP-IDF 6.0.1 后，构建并签署 Muse：

```sh
bash esp32/tools/pocket/build.sh
.venv/bin/python esp32/tools/pocket/coexist_certificate.py stamp \
  esp32/build-xteink-s3/muse-gadget.bin artifacts/crossmux-custom.bin \
  artifacts/muse-custom-uncredentialed.bin \
  --keyfile private/coexist-signing.pem --version 0.1.5-coexist-zh
python3 esp32/tools/pocket/package_private.py \
  artifacts/muse-custom-uncredentialed.bin artifacts/muse-custom-private.bin
```

最后一条通过隐藏提示输入自己的 Muse SDK token。个人 `.bin` 含凭据，不能公开。普通源码构建只有空证书槽，必须先签署再加入 token。发布模板已经签署，普通安装者只需私人打包步骤。私钥丢失后无法为原 CrossMux 签署新模板，应重新生成并验证一整套固件。
