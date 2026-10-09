"""Execute the same C gesture state machine used by button.c."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class GestureTest(unittest.TestCase):
    def test_poll_edges_and_board_thresholds(self):
        self.assertTrue((ROOT / "main/pocket_gesture.h").exists(), "poll-safe gesture helper is missing")
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "gesture"
            subprocess.run([os.getenv("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                            "-fsanitize=address,undefined", "-I", str(ROOT / "main"),
                            str(Path(__file__).with_name("gesture_harness.c")), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
