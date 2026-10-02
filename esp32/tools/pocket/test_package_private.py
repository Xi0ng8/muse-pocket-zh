"""Focused checks for the private firmware's token and image integrity."""
import hashlib
from pathlib import Path
import stat
import struct
import subprocess
import sys
import tempfile
import unittest
from package_private import SLOT, image_layout, package


def image(chip=9, payload=SLOT):
    header = bytearray(24)
    header[0:2] = bytes([0xE9, 1])
    struct.pack_into("<H", header, 12, chip)
    header[23] = 1
    data = header + struct.pack("<II", 0x3C020000, len(payload)) + payload
    checksum = 0xEF
    for value in payload:
        checksum ^= value
    data += bytes(15 - len(data) % 16) + bytes([checksum])
    data += hashlib.sha256(data).digest()
    return data


class PrivateFirmware(unittest.TestCase):
    def test_cli_stdin_is_private_and_does_not_replace_an_output(self):
        token = "mgst_" + "A" * 43
        script = Path(__file__).with_name("package_private.py")
        with tempfile.TemporaryDirectory() as tmp:
            source, output = Path(tmp) / "input.bin", Path(tmp) / "private.bin"
            source.write_bytes(image())
            args = [sys.executable, str(script), str(source), str(output), "--token-stdin"]
            result = subprocess.run(args, input=token, text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertNotIn(token, result.stdout + result.stderr)
            self.assertEqual(stat.S_IMODE(output.stat().st_mode), 0o600)
            self.assertEqual(source.read_bytes(), image())
            original = output.read_bytes()
            again = subprocess.run(args, input=token, text=True, capture_output=True)
            self.assertNotEqual(again.returncode, 0)
            self.assertNotIn(token, again.stdout + again.stderr)
            self.assertEqual(output.read_bytes(), original)

    def test_cli_rejects_a_bad_token_without_printing_it(self):
        token = "mgst_" + "B" * 43
        script = Path(__file__).with_name("package_private.py")
        with tempfile.TemporaryDirectory() as tmp:
            source, output = Path(tmp) / "input.bin", Path(tmp) / "private.bin"
            source.write_bytes(image())
            result = subprocess.run(
                [sys.executable, str(script), str(source), str(output), "--token-stdin"],
                input=token, text=True, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertNotIn(token, result.stdout + result.stderr)
            self.assertFalse(output.exists())

    def test_token_patch_preserves_layout_and_validates_integrity(self):
        source = image()
        token = "mgst_" + "A" * 43
        result = package(source, token)
        self.assertEqual(len(source), len(result))
        self.assertEqual(source[:32], result[:32])
        self.assertEqual(result[32:80], token.encode())
        self.assertEqual(image_layout(source), image_layout(result))
        self.assertNotIn(SLOT, result)

    def test_rejects_wrong_chip_corruption_and_repackaging(self):
        token = "mgst_" + "A" * 43
        with self.assertRaisesRegex(ValueError, "ESP32-S3"):
            package(image(chip=13), token)
        corrupt = image()
        corrupt[40] ^= 1
        with self.assertRaisesRegex(ValueError, "checksum"):
            package(corrupt, token)
        with self.assertRaisesRegex(ValueError, "unmodified"):
            package(package(image(), token), token)

    def test_rejects_ambiguous_slot_and_noncanonical_token(self):
        with self.assertRaisesRegex(ValueError, "exactly one"):
            package(image(payload=SLOT * 2), "mgst_" + "A" * 43)
        with self.assertRaisesRegex(ValueError, "canonical"):
            package(image(), "mgst_" + "A" * 42 + "B")


if __name__ == "__main__":
    unittest.main()
