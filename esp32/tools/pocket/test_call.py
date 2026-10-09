"""Run the actual bounded call state machine with host transport/UI doubles."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]
class CallTests(unittest.TestCase):
    def test_call_state_and_late_transport_events(self):
        self.assertTrue((ROOT/"main/pocket_call.c").exists(), "call state machine is missing")
        with tempfile.TemporaryDirectory() as directory:
            binary=Path(directory)/"call"
            subprocess.run([os.environ.get("CC","cc"),"-std=c11","-D_POSIX_C_SOURCE=200809L",
                "-Wall","-Wextra","-Werror","-fsanitize=address,undefined","-g",
                "-I",str(Path(__file__).with_name("call_stubs")),"-I",str(ROOT/"main"),
                str(ROOT/"main/pocket_call.c"),str(Path(__file__).with_name("call_harness.c")),"-o",str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
if __name__=="__main__":unittest.main()
