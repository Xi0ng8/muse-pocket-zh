// Copyright (c) Muse Pocket contributors. Licensed under Apache-2.0.
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// 16 rows, 2 bytes per row, MSB first. Set bits are ink.
// A supported blank glyph has a non-null pointer to 32 zero bytes.
// Unsupported codepoints return nullptr (NULL in C).
const uint8_t* pocket_cjk_glyph(uint32_t cp);
#ifdef __cplusplus
}
#endif
