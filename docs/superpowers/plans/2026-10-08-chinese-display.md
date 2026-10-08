# Muse Pocket Chinese Display Implementation Plan

**Goal:** Render Chinese Muse names and status text, provide Chinese-first menus, and publish an auditable Chinese fork without an upstream PR.

**Architecture:** Keep the ESP32-S3 app-only installation, credential slot, recovery SHA256 and pairing storage unchanged. Add a bounded UTF-8 layout module with pixel-width wrapping and an embedded OFL bitmap font. Simplified Chinese is the default UI, with a persisted optional language selector. Generate the CJK bitmap at build-development time; firmware requires no font library or SD access.

**Tech Stack:** Existing ESP-IDF 6.0.1, host C++ tests, existing Python tooling and OFL CJK font assets.

## Tasks
- [x] Compare public forks and searches for an existing Chinese implementation; reuse an appropriate implementation if present.
- [x] Font lane: generate 16x16 monochrome CJK Unified Ideographs, CJK punctuation and fullwidth glyphs; include upstream source hash, license and reproducible generator. Expose `const uint8_t* pocket_cjk_glyph(uint32_t codepoint)`; 32 bytes per glyph, row-major, MSB first. Return null for uncovered/missing glyphs.
- [x] Renderer lane: first add host tests for mixed Chinese/Latin widths, UTF-8 decoding, invalid/truncated sequences, wrapping, newline, safe truncation and missing glyphs. Verify tests fail; implement bounded text layout and rendering. Update name centering, four-line captions and stored strings to avoid partial UTF-8. Add Chinese-first UI selection persisted in muse_pocket NVS without altering credential storage.
- [x] Update command descriptions and the startup prompt to allow Chinese; no shared protocol changes beyond descriptions.
- [x] Add Chinese README and build/install/use docs; accurately label host/build verification versus untested Chinese hardware behavior.
- [x] Run focused host tests, original packaging/recovery tests, X4Pro build, whitespace checks and public-tree/history checks. Inspect all staged content and license notices.
- [ ] Create a public GitHub fork under the active account, publish the implementation branch and do not open an upstream PR. Publish a prerelease with only the validated uncredentialed app image, hash and local packaging instructions. Publish directly as a Chinese-first community fork.
- [ ] Install privately only after obtaining a physical file-transfer address; retain user's token locally and use exclusive new output filenames. Request physical SD update and panel verification when needed.

## Acceptance
Mixed Chinese/English captions wrap by pixel width, names center by displayed width, malformed UTF-8 cannot over-read buffers, and Chinese menus fit 480x800. Unsupported emoji use one fallback per codepoint. Existing CrossPoint recovery pin is byte-for-byte unchanged. Published assets contain no SDK/account/Wi-Fi secrets. Hardware results must be confirmed on the user's device, not inferred from build success.
