# Pocket CJK bitmap font

`pocket_cjk_data.inc` is a modified, rasterized subset of **Noto Sans CJK SC
Regular**, copyright © 2014–2021 Adobe (http://www.adobe.com/). The font data
is distributed under the SIL Open Font License 1.1; the complete upstream
license is in [OFL.txt](OFL.txt). The derived font is named Pocket CJK.
The generator and lookup source are Apache-2.0; this does not change the OFL
license of the bitmap data.

Upstream repository: https://github.com/notofonts/noto-cjk

Pinned upstream commit: `f8d157532fbfaeda587e826d4cd5b21a49186f7c`.

Source font URL:
https://raw.githubusercontent.com/notofonts/noto-cjk/f8d157532fbfaeda587e826d4cd5b21a49186f7c/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf

Source SHA256:
`2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b`.

Upstream license URL:
https://raw.githubusercontent.com/notofonts/noto-cjk/f8d157532fbfaeda587e826d4cd5b21a49186f7c/Sans/LICENSE

The 21,284 supported codepoints come from the source Unicode cmap within
U+4E00–U+9FFF, U+3000–U+303F, U+FF00–U+FFEF and U+2010–U+2027. Unsupported cmap entries are
omitted, not rendered as `.notdef`. Each glyph is 16 × 16 monochrome pixels,
16 rows of two bytes, with the high bit on the left and set bits representing
ink. Blank supported glyphs (U+3000 and U+FFA0) have zero data but still return
a valid pointer. Missing characters return `nullptr`.

The constant bitmap array is 681,088 bytes and its sorted uint16 codepoint
index is 42,568 bytes: 723,656 bytes of read-only font data. A binary search
returns a direct pointer into this data. There is no heap use or runtime
font decompression. `metadata.json` records coverage, adjustments, generation
parameters and bitmap SHA256. Generated C source text is larger than the
embedded binary data.

See [generation instructions](../../../tools/fonts/README.md).
