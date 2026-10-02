// Copyright (c) 2026 Muse Pocket contributors. SPDX-License-Identifier: Apache-2.0
// Supplies the retained SDK renderer interface; Muse Pocket uses happy_anim.c.
#include "muse_pixel.h"
#include <string.h>

static uint8_t pixels[MUSE_PX_W * MUSE_PX_H];
static int output_size = MUSE_PX_W;

uint32_t muse_pixel_accent(muse_mode_t mode) {
    return mode == MUSE_MODE_ERROR ? 0xff4040 : 0x808080;
}
void muse_pixel_set_size(int px) {
    output_size = px < 1 ? 1 : (px > 512 ? 512 : px);
}
void muse_pixel_render(const muse_pose_t *pose) {
    (void)pose;
    memset(pixels, 0, sizeof(pixels));
    for (int y = 0; y < MUSE_PX_H; ++y) {
        for (int x = 0; x < MUSE_PX_W; ++x) {
            int outline = x >= 10 && x <= 53 && y >= 10 && y <= 51
                && (x <= 11 || x >= 52 || y <= 11 || y >= 50);
            int eyes = ((x >= 22 && x <= 26) || (x >= 37 && x <= 41))
                && y >= 25 && y <= 30;
            int mouth = x >= 26 && x <= 37 && y == 39;
            int stem = x >= 29 && x <= 34 && y >= 52 && y <= 56;
            int foot = x >= 22 && x <= 41 && y >= 57 && y <= 58;
            pixels[y * MUSE_PX_W + x] = outline || eyes || mouth || stem || foot;
        }
    }
}
void muse_pixel_scale(uint16_t *dst, int stride_px, int x0, int x1, int y0, int y1) {
    if (!dst || stride_px <= 0) return;
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            int gx = x * MUSE_PX_W / output_size;
            int gy = y * MUSE_PX_H / output_size;
            uint16_t color = 0xffff;
            if (gx >= 0 && gx < MUSE_PX_W && gy >= 0 && gy < MUSE_PX_H)
                color = pixels[gy * MUSE_PX_W + gx] ? 0x0000 : 0xffff;
            dst[(y - y0) * stride_px + x - x0] = color;
        }
    }
}
