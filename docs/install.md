# Install, pair and recover

This guide is for the **Xteink X4 Pro**, with **CrossPoint 1.6.5 for X4 Pro**
installed first. The X4 and X3 are different devices. Get CrossPoint and its
current installation instructions from the [official installer](https://crosspointreader.com).
The project's cable-free initial installation was tested through the official
Xteink Unlocker on macOS, starting from stock XT V7.4.4.

Keep the reader charged during updates. Use the [private app image you built](build.md),
not a bootloader, partition table, merged flash image or someone else's binary.

## Copy the file to microSD

Either copy the private `.bin` directly onto the microSD card, or use CrossPoint:

1. Open **File Transfer → Join Network** on the reader.
2. Join a Wi-Fi network your computer can also reach.
3. Open the URL shown on the reader in your browser. Use that actual address;
   it changes between networks and devices.
4. Upload your private `.bin` into the card's root directory.
5. Download it back and compare its size and SHA256 with the packager's output.

For agents using the file-transfer API, see [AGENTS.md](../AGENTS.md). Do not
overwrite an existing file unless you know what it is; use a new name instead.

## Install from the reader

1. Leave File Transfer.
2. Open **Settings → System → SD Card Firmware Update**.
3. Select the exact private `.bin` you just verified and confirm.
4. Let the update finish without turning the reader off.

CrossPoint writes the inactive firmware slot. Its own application stays in the
other slot. Muse Pocket does not replace the reader's bootloader or partition
table, and it keeps existing pairing and Wi-Fi storage.

On first boot, press **Right** to open Muse Pocket Settings. The recovery row
should say **Return to CrossPoint**. If it says **CrossPoint not verified**, stop
and check the installed version and build: this firmware deliberately confirms
only the exact official CrossPoint 1.6.5 X4 Pro image. Do not bypass the check.
A new image whose local startup check fails can return to CrossPoint after five
minutes. Initial Muse pairing does not need to finish within that time.

## Pair with your Muse

1. Open the Muse app and enable **Developer mode** in its device settings.
2. Use **Add gadget** and select the reader's `MuseGadget…Pocket` advertisement.
3. Follow the app's community-device pairing flow. Review the access it grants
   to the device; pair only firmware you trust.
4. When the reader asks, press **Left** to confirm the physical device.
5. Select your Wi-Fi network and complete setup.

Once connected, the name comes from your paired Muse. The reader asks that Muse
to send its character and a status caption. If only the neutral icon appears,
see [display troubleshooting](usage.md#troubleshooting).

## Return to CrossPoint

In Muse Pocket Settings, select **Return to CrossPoint** and hold **Power for
3 seconds**. For the startup recovery path, hold **Right while starting** the
reader; keep it held for at least 1.2 seconds.

Recovery checks the other firmware slot against this exact official image:

- Version: CrossPoint 1.6.5, X4 Pro.
- Length: `5632640` bytes.
- SHA256: `9ebd6ef1e0bb39ff8dcbff3947f938cb6158a1bcb1769d3811cb8bc6c6667eab`.

It does not rely on a generic application name or switch to an unknown slot.
The startup path runs before Muse's network, storage and display setup, so it can
work when those fail. It still needs the pinned CrossPoint image to be present.

To update or reinstall Muse Pocket later, return to CrossPoint and repeat the
SD-card process. **Do not install another firmware over the recovery slot from
inside Muse Pocket.** Its remote firmware updater is disabled for this reason.

The return path preserves CrossPoint, not a separate backup of the factory Xteink
firmware. Use official Xteink/CrossPoint recovery instructions if you need stock
firmware again. Hardware recovery may require the magnetic USB adapter.
