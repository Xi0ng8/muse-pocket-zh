# Display, settings and Muse commands

Muse Pocket is a companion screen. It receives a character image and caption
through the Muse Gadget SDK's paired connection; it does not expose a public web
API or poll a separate activity feed.

## Settings and controls

Press **Right** on the main screen to open Settings, then press Right to move
through the rows. Press **Power** to change the selected setting.

| Setting | Choices |
| --- | --- |
| Brightness | Frontlight percentage |
| Warmth | Cool/warm balance |
| Refresh | 2, 5, 15 or 30 seconds |
| Orientation | Normal or flipped |
| Sleep | Preserve the ink and stop live updates |
| Return to CrossPoint | Hold Power for 3 seconds to return |
| Back to Muse | Return to the companion screen |

Brightness, warmth, refresh and orientation are saved in the separate
`muse_pocket` settings area. Power wakes a sleeping reader. On the main screen,
Power performs a clean refresh; holding it for 3 seconds sleeps the device.
Left confirms pairing or retries setup. Double-tapping Left rescans Wi-Fi;
holding it for 5 seconds resets Muse setup and forgets its Wi-Fi credentials.

Captions are batched according to the refresh setting. Image changes use a full
refresh; the display also performs a full refresh after ten incremental updates.

## Commands your Muse can call

| Command | Parameters | Result |
| --- | --- | --- |
| `display.draw_url` | `url`, optional `row` | Draw an image; keep the caption |
| `pocket.set_status` | `text`, up to 240 UTF-8 bytes | Set the caption below the character |
| `pocket.set_frontlight` | `brightness`, `warmth`, both 0–100 | Change and save the frontlight |
| `display.show_animation` | None | Return to the neutral placeholder icon |

Use a **baseline JPEG** prepared for the **480×480 character canvas**, or
big-endian RGB565 data. Gray is converted to black and white with dithering.
An incomplete image download leaves the previous character visible.
Captions show up to four lines of 35 ASCII characters each. Longer text can be
accepted up to the byte limit, but only the visible lines are drawn. Keep status
updates short and use plain ASCII; accented letters and emoji become `?`.

Example command parameters:

```json
{"text": "Reading your notes.\nNext: drafting a reply."}
```

```json
{"brightness": 25, "warmth": 50}
```

## Automatic character and status setup

After the paired session registers, the firmware sends one message to the Muse's
main chat, using the SDK's `/chat/stream` protocol. It asks the Muse to send its
own character through `display.draw_url` and update `pocket.set_status` when its
activity changes. A successfully accepted request is remembered for that boot
and Muse, so ordinary reconnects do not repeat it. Restarting asks again because
the current character is held in memory.

This is a request to the Muse, not a guaranteed activity subscription. The Muse
must execute the display commands and decide when its activity has changed.

## Troubleshooting

- **Connected, but no character:** ask your Muse to send its own character as a
  480×480 baseline JPEG using `display.draw_url`, then set the current activity
  with `pocket.set_status`. Ask it to tell you if either command fails.
- **Name still says Muse Pocket:** the paired identity request has not returned.
  Check the Muse connection and wait for reconnect.
- **Caption stopped changing:** the Muse must send a new status command. It is
  not a timer that invents new activities.
- **Reconnecting:** check the chosen Wi-Fi network and the Muse service. Your
  previous image and caption remain visible during ordinary reconnects.
- **CrossPoint not verified:** follow the [recovery guide](install.md); do not
  weaken the pinned-image check to make the warning disappear.
- **Screen is blank or stuck:** the driver stops after an inconclusive panel
  probe or a busy timeout. Try the held-Right startup recovery path. Do not
  blindly flash a different model's pin map or partition table.

For a bug report, include the X4 Pro panel variant if known, source version, what
you pressed and a photo of the screen. Review logs before sharing. Never attach
a private `.bin`, SDK token, device token, Wi-Fi password or generated `sdkconfig`.
