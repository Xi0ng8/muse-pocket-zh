Driver and panel probe source: https://github.com/Free-Ink/freeink-sdk

Pinned commit: 6af90c4c50b4e067516bb4c344bb686a26f1da52

MIT license preserved in LICENSE. Driver files and waveform tables are copied unchanged. The probe includes only the S3-safe shared display probe. Native ESP-IDF replaces Arduino SPI/GPIO with the local compatibility layer. No C3 board-detection code or USB GPIO probing is compiled.
