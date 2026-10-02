"""Keep the recovery pin tied to the independently verified official image."""
import re
import unittest
from pathlib import Path


class RecoveryPinTest(unittest.TestCase):
    def test_official_crosspoint_image_pin(self):
        # Official stable-1.6.5-x4pro download and decrypted OTA package agreed.
        expected = "9ebd6ef1e0bb39ff8dcbff3947f938cb6158a1bcb1769d3811cb8bc6c6667eab"
        source = (Path(__file__).resolve().parents[2] / "main/pocket_recovery.c").read_text()
        initializer = re.search(r"crosspoint_sha\[32\] = \{([^}]+)\}", source, re.S).group(1)
        actual = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})", initializer))
        self.assertEqual(actual.hex(), expected)
        self.assertIn("crosspoint_bytes = 5632640;", source)


if __name__ == "__main__":
    unittest.main()
