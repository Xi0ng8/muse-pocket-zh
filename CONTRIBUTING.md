# Contributing

Keep changes focused on the X4 Pro companion. Describe what the user should see,
why it changed and what you actually verified. Ask about an uncertain hardware
variant before changing its pins or recovery layout.

## Local checks

With ESP-IDF 6.0.1 activated:

```sh
esp32/tools/pocket/build.sh
python3 -m unittest discover -s esp32/tools/pocket -p 'test_*.py' -v
python3 tools/check_public_tree.py
git diff --check
```

The focused tests verify private packaging integrity, rejection of wrong-chip or
corrupted images, and the recovery digest. They do not verify a physical panel,
the Muse service or a real recovery round trip.

For shared SDK changes, run the relevant tests in `esp32/tests` too. The full
suite compiles host C/C++ harnesses; it needs `cc`, `c++`, `pkg-config`, a host PSA
Crypto/mbedTLS library, and managed components fetched by an IDF build:

```sh
cd esp32
python3 -m unittest discover -s tests -p 'test_*.py' -v
```

The Pocket CI workflow builds the X4 Pro and runs the focused and SDK host tests
without an SDK token. Do not put user credentials into CI or upload packaged private firmware
as an Actions artifact. Changes that need hardware should say which panel was
tested and whether the evidence is a log, a photo or a user's confirmation.

## Before opening an issue or pull request

Review every staged file. Keep private images, generated `sdkconfig`, account
tokens, Wi-Fi details and device logs out of commits and attachments. The public
tree checker also scans reachable Git history: deleting a credential in a later
commit does not remove its earlier exposure. If a credential is exposed, revoke
it and arrange a proper history cleanup; do not quote it in the issue.

Use your own Muse and token for testing. A contributor's build should not change
another user's account, pairing, computer settings or device without authorization.

## Upstream and licenses

The firmware contains a modified snapshot of the Muse Gadget SDK and a pinned
MIT subset of FreeInk. Preserve the licenses and notices in [NOTICE](NOTICE).
Mark changes to inherited files. Keep proprietary avatar art and users' generated
characters out of the source tree.
