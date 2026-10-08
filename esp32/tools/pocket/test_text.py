"""Exercise the actual firmware text engine on the host, without ESP-IDF."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class TextTests(unittest.TestCase):
    def test_utf8_and_pixel_layout(self):
        source = ROOT / "main" / "pocket_text.cpp"
        self.assertTrue(source.exists(), "bounded UTF-8 text engine is missing")
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "text_harness"
            command = [os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra",
                       "-Werror", "-fsanitize=address,undefined", "-g", "-I", str(ROOT / "main"),
                       str(source), str(Path(__file__).with_name("text_harness.cpp")), "-o", str(binary)]
            subprocess.run(command, check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
