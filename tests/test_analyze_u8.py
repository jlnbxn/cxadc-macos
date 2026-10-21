# SPDX-License-Identifier: GPL-2.0-or-later
import importlib.util
import math
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("analyze_u8", Path(__file__).resolve().parents[1] / "tools/analyze_u8.py")
analyzer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analyzer)


class Statistics(unittest.TestCase):
    def run_file(self, data):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fixture.bin"
            path.write_bytes(data)
            return analyzer.analyze(path, 256)

    def test_all_codes(self):
        result = self.run_file(bytes(range(256)))
        self.assertEqual(result["histogram"], [1] * 256)
        self.assertEqual(result["mean_codes"], 127.5)
        self.assertAlmostEqual(result["standard_deviation_codes"], math.sqrt((256**2 - 1) / 12))
        self.assertEqual(result["percent_at_0"], 100 / 256)
        self.assertEqual(result["percent_at_255"], 100 / 256)
        self.assertEqual(result["inferred_duration_seconds"], 1)

    def test_constant(self):
        result = self.run_file(bytes([128]) * 4096)
        self.assertEqual(result["standard_deviation_codes"], 0)
        self.assertEqual(result["rms_including_dc_codes"], 128)
        self.assertIsNone(result["approximate_span_db_relative_to_one_code"])

    def test_empty(self):
        with self.assertRaises(ValueError):
            self.run_file(b"")


if __name__ == "__main__":
    unittest.main()
