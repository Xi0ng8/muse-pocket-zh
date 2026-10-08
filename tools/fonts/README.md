# Reproduce Pocket CJK

The committed font data is derived from OFL Noto Sans CJK SC. Attribution,
source URLs and the full license are in `esp32/main/fonts/`. The source font
and preview belong in the ignored `artifacts/fonts/` directory, not Git.

Run from the repository root, with existing Python 3.12, Pillow 10.4.0,
fontTools 4.59.1 and Pillow's FreeType 2.13.2. The generator rejects different
versions or a source SHA256 mismatch rather than silently changing the font.
It does not download or install dependencies.

```sh
mkdir -p artifacts/fonts
curl -fL https://raw.githubusercontent.com/notofonts/noto-cjk/f8d157532fbfaeda587e826d4cd5b21a49186f7c/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf -o artifacts/fonts/NotoSansCJKsc-Regular.otf
python3 tools/fonts/generate_cjk.py
python3 -m unittest discover -s tools/fonts -p 'test_*.py' -v
```

Generation uses Pillow BASIC layout, size 15, a 16 × 16 cell and baseline 13.
The grayscale raster is thresholded at 96, then packed row-major MSB-first.
Rendering on a padded surface preserves ink outside the nominal advance.
Ordinary glyphs retain their baseline and left bearing. The few overflowing
symbols are shifted minimally into the cell; oversized ink is scaled with
nearest-neighbor sampling before placement. This avoids clipping punctuation,
combining marks and full-width Latin descenders. General punctuation U+2010–U+2027 (including curly quotes and ellipsis) is
also included for Chinese sentences. The source cmap distinguishes
missing characters from supported empty glyphs. `metadata.json` lists any
missing or adjusted codepoints.

The preview `artifacts/fonts/cjk-preview.png` shows representative Chinese
characters, dense strokes, punctuation and the supported ideographic space
at 4× scale. Inspect it after changing generation parameters. The glyph data
and metadata can be reproduced into a temporary directory using
`--output-dir PATH --preview PATH`; these flags do not change raster parameters.
The tests build the C++ lookup as a host library, exhaustively check requested
coverage including misses, check blank-vs-missing behavior, verify bitmap
SHA256, and (when the local source font exists) verify byte-identical generation.
