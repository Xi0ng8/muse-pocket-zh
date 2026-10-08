# Copyright (c) Muse Pocket contributors. Licensed under Apache-2.0.
import ctypes
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
FONT_DIR = ROOT / "esp32/main/fonts"
RANGES = ((0x2010, 0x2027), (0x3000, 0x303F), (0x4E00, 0x9FFF), (0xFF00, 0xFFEF))


class CjkFontTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        library = Path(cls.temp.name) / "font.so"
        subprocess.run(["c++", "-std=c++11", "-Wall", "-Wextra", "-Werror", "-shared", "-fPIC",
                        str(ROOT / "esp32/main/pocket_cjk_font.cpp"), "-o", str(library)], check=True)
        cls.library = ctypes.CDLL(str(library))
        cls.lookup = cls.library.pocket_cjk_glyph
        cls.lookup.argtypes = [ctypes.c_uint32]
        cls.lookup.restype = ctypes.POINTER(ctypes.c_uint8)
        cls.metadata = json.loads((FONT_DIR / "metadata.json").read_text())
        index = (FONT_DIR / "pocket_cjk_data.inc").read_text().split("};", 1)[0]
        cls.codepoints = [int(v, 16) for v in re.findall(r"0x([0-9a-f]{4})", index)]

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def test_exhaustive_requested_coverage_and_missing(self):
        self.assertEqual(self.codepoints, sorted(set(self.codepoints)))
        supported = set(self.codepoints)
        missing = set()
        for first, last in RANGES:
            for cp in range(first, last + 1):
                self.assertEqual(bool(self.lookup(cp)), cp in supported, hex(cp))
                if cp not in supported:
                    missing.add(f"U+{cp:04X}")
        self.assertEqual(missing, set(self.metadata["missing_codepoints"]))
        self.assertEqual(len(supported), 21284)
        for cp in (0, 0x41, 0x2FFF, 0x3040, 0x4DFF, 0xA000, 0xFEFF, 0xFFF0, 0x10000, 0x1F600, 0xFFFFFFFF):
            self.assertFalse(self.lookup(cp), hex(cp))

    def test_bitmap_integrity_and_blank_distinction(self):
        data = bytearray()
        blanks = []
        for cp in self.codepoints:
            glyph = bytes(self.lookup(cp)[:32])
            data.extend(glyph)
            if not any(glyph):
                blanks.append(f"U+{cp:04X}")
        self.assertEqual(len(data), self.metadata["bitmap_bytes"])
        self.assertEqual(hashlib.sha256(data).hexdigest(), self.metadata["bitmap_sha256"])
        self.assertEqual(blanks, ["U+3000", "U+FFA0"])
        self.assertEqual(blanks, self.metadata["blank_codepoints"])
        for cp in map(ord, "中文你好世界状态设置恢复，。！？（）他说：“你好……”"):
            self.assertTrue(any(self.lookup(cp)[:32]), hex(cp))

    def test_reproducible_generation_from_local_source(self):
        source = ROOT / "artifacts/fonts/NotoSansCJKsc-Regular.otf"
        if not source.exists():
            self.skipTest("Ignored original font absent; see tools/fonts/README.md")
        output = Path(self.temp.name) / "regenerated"
        subprocess.run([sys.executable, str(ROOT / "tools/fonts/generate_cjk.py"),
                        "--font", str(source), "--output-dir", str(output),
                        "--preview", str(Path(self.temp.name) / "preview.png")], check=True)
        for filename in ("pocket_cjk_data.inc", "metadata.json"):
            self.assertEqual((output / filename).read_bytes(), (FONT_DIR / filename).read_bytes())


if __name__ == "__main__":
    unittest.main()
