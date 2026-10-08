// Copyright (c) Muse Pocket contributors. Licensed under Apache-2.0.
#include "pocket_cjk_font.h"
#include <stddef.h>

namespace {
#include "fonts/pocket_cjk_data.inc"
static_assert(sizeof(kCjkBitmaps) / sizeof(kCjkBitmaps[0]) ==
              sizeof(kCjkCodepoints) / sizeof(kCjkCodepoints[0]), "font index mismatch");
}

const uint8_t* pocket_cjk_glyph(uint32_t cp) {
    size_t first = 0;
    size_t last = sizeof(kCjkCodepoints) / sizeof(kCjkCodepoints[0]);
    while (first < last) {
        const size_t mid = first + (last - first) / 2;
        if (kCjkCodepoints[mid] < cp) first = mid + 1;
        else last = mid;
    }
    if (first < sizeof(kCjkCodepoints) / sizeof(kCjkCodepoints[0]) && kCjkCodepoints[first] == cp)
        return kCjkBitmaps[first];
    return nullptr;
}
