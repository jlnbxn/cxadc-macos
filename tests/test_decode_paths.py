#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Executable lookup tests; no external tool or hardware is run."""
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from decode_full_tape import executable


class DecodePaths(unittest.TestCase):
    def test_path_tool_takes_precedence(self):
        with patch("decode_full_tape.shutil.which", return_value="/bin/sh"):
            self.assertEqual(executable(None, "tool", Path("missing")), str(Path("/bin/sh").resolve()))

    def test_explicit_missing_tool_does_not_use_fallback(self):
        with patch("decode_full_tape.shutil.which", return_value=None):
            with self.assertRaisesRegex(ValueError, "explicit path"):
                executable("missing-tool", "tool", Path("/bin/sh"))

    def test_local_fallback_requires_executable_permission(self):
        with tempfile.TemporaryDirectory() as directory:
            file = Path(directory) / "tool"
            file.write_text("test fixture, never executed")
            file.chmod(0o600)
            with patch("decode_full_tape.shutil.which", return_value=None):
                with self.assertRaises(ValueError):
                    executable(None, "tool", file)
                file.chmod(0o700)
                self.assertEqual(executable(None, "tool", file), str(file.resolve()))


if __name__ == "__main__":
    unittest.main()
