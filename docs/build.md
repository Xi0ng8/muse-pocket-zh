# Build your own firmware

ESP-IDF is Espressif's set of tools for compiling ESP32 firmware. Muse Pocket
requires **version 6.0.1** and targets the **ESP32-S3** inside the X4 Pro.
Use macOS or Linux, or a Linux environment on Windows such as WSL.

## 1. Install and activate ESP-IDF

Follow Espressif's [ESP-IDF setup guide](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32s3/get-started/index.html).
Install the `v6.0.1` release and its ESP32-S3 tools. Keep the toolchain outside
the repository. Activate it in the terminal you will use to build:

```sh
source /path/to/esp-idf/export.sh
idf.py --version
```

Replace `/path/to/esp-idf` with your actual installation directory. The version
output must include `v6.0.1`.

## 2. Get the source and build

```sh
git clone https://github.com/viticci/muse-pocket.git
cd muse-pocket
esp32/tools/pocket/build.sh
python3 -m unittest discover -s esp32/tools/pocket -p 'test_*.py' -v
```

The application image is `esp32/build-xteink-s3/muse-gadget.bin`. It contains a
reserved empty token field and cannot pair until you package a private copy.
Do not put the token into source, `sdkconfig`, shell arguments or a commit.

The build helper selects the X4 Pro settings and a separate build directory. It
does **not** flash the device. A new checkout fetches its managed dependencies
during the first build, so that step needs Internet access.

## 3. Add your SDK token locally

Create your own token at [Muse SDK tokens](https://gadgets.muse.ai/settings/sdk-tokens),
then run:

```sh
python3 esp32/tools/pocket/package_private.py \
  esp32/build-xteink-s3/muse-gadget.bin \
  artifacts/muse-pocket-x4-pro-private.bin
```

Paste the token at the hidden prompt. It is not echoed or printed. The packager
holds it in memory, patches one reserved field, and recalculates the ESP image's
checksum and SHA256 digest. The input build remains uncredentialed. The private
output is created with owner-only permissions and never replaces an existing file.

For an automated local workflow, `--token-stdin` reads from a pipe instead of a
prompt. Pipe the output of your own trusted token source directly into the
command. Do not put the literal token in an `echo` command or shell history.
No particular password manager is required.

The output's size and SHA256 identify the exact file to transfer. Save those
values locally. The private file **contains your SDK token**; `.gitignore` is a
convenience, not permission to upload it. Do not publish it or firmware logs
without reviewing them for credentials and network information.

If the output already exists, use a new filename. Keep a known working private
image locally if you want to reinstall after returning to CrossPoint.

## 4. Validate the image

In the activated ESP-IDF terminal:

```sh
python -m esptool --chip esp32s3 image-info \
  artifacts/muse-pocket-x4-pro-private.bin
```

Check that it reports **ESP32-S3**, a valid checksum and a valid validation hash.
The packager also rejects a wrong-chip, corrupted or already packaged input.
Now follow [installation](install.md).

## Troubleshooting a build

- **`idf.py` missing:** activate the installed ESP-IDF environment in this terminal.
- **Wrong IDF version:** use 6.0.1; newer versions are not qualified here.
- **Wrong target or settings:** use the Pocket helper, not the upstream SDK's
  default build, which targets a different board.
- **Old generated settings:** a build directory's `sdkconfig` can override changed
  defaults. Preserve anything you need, then create a clean build directory for
  the X4 Pro. Do not copy tokens into it.
- **Private terminal unavailable:** use a trusted pipe with `--token-stdin`.

Never run the generated `idf.py flash` instructions for this reader. They write
the bootloader and partition table. Muse Pocket uses the app-only SD update.
