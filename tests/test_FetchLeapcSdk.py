"""Offline SDK-fetch verification; no proprietary SDK content or network required."""
import hashlib
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("fetch_leapc_sdk", ROOT / "scripts" / "fetch-leapc-sdk.py")
FETCH = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FETCH)


class FetchSdkTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="handbridge-sdk-fetch-")
        self.destination = Path(self.temporary.name)
        self.target = self.destination / "include" / "LeapC.h"
        self.target.parent.mkdir()
        self.payload = b"synthetic test input, not an SDK header"
        self.entry = {
            "path": "include/LeapC.h",
            "url": "https://s3.eu-west-1.amazonaws.com/downloads.ultraleap.com/test/LeapC.h",
            "bytes": len(self.payload),
            "sha256": hashlib.sha256(self.payload).hexdigest(),
        }
        self.addCleanup(self.temporary.cleanup)

    def test_verified_cache_does_not_access_network(self):
        self.target.write_bytes(self.payload)
        with patch.object(FETCH, "urlopen") as network:
            self.assertIn("cached", FETCH.fetch(self.entry, self.destination))
            network.assert_not_called()

    def test_valid_download_atomically_replaces_bad_cache(self):
        self.target.write_bytes(b"previous bad cached content")
        with patch.object(FETCH, "urlopen", return_value=io.BytesIO(self.payload)):
            FETCH.fetch(self.entry, self.destination)
        self.assertEqual(self.target.read_bytes(), self.payload)
        self.assertEqual(list(self.target.parent.glob("*.tmp")), [])

    def test_digest_or_size_failure_keeps_previous_file(self):
        self.target.write_bytes(b"preserve this previous file")
        for payload in (b"x" * len(self.payload), self.payload[:-1], self.payload + b"extra"):
            with self.subTest(size=len(payload)):
                with patch.object(FETCH, "urlopen", return_value=io.BytesIO(payload)):
                    with self.assertRaises(ValueError):
                        FETCH.fetch(self.entry, self.destination)
                self.assertEqual(self.target.read_bytes(), b"preserve this previous file")
                self.assertEqual(list(self.target.parent.glob("*.tmp")), [])

    def test_network_failure_keeps_previous_file_and_removes_temporary(self):
        self.target.write_bytes(b"preserve me")
        with patch.object(FETCH, "urlopen", side_effect=OSError("injected download failure")):
            with self.assertRaises(OSError):
                FETCH.fetch(self.entry, self.destination)
        self.assertEqual(self.target.read_bytes(), b"preserve me")
        self.assertEqual(list(self.target.parent.glob("*.tmp")), [])


    def test_include_filters_use_the_extractors_native_separator(self):
        entries = [{"installer_path": "app/LeapSDK/include/LeapC.h"},
                   {"installer_path": "app/LeapSDK/lib/x64/LeapC.dll"}]
        for windows, expected in ((True, [r"app\LeapSDK\include\LeapC.h", r"app\LeapSDK\lib\x64\LeapC.dll"]),
                                  (False, ["app/LeapSDK/include/LeapC.h", "app/LeapSDK/lib/x64/LeapC.dll"])):
            with self.subTest(windows=windows):
                command = FETCH.extraction_command(Path("tool"), self.destination, Path("installer.exe"), entries, windows)
                includes = [command[index + 1] for index, value in enumerate(command) if value == "--include"]
                self.assertEqual(includes, expected)
                self.assertEqual(command[-1], "installer.exe")

    def test_missing_extraction_diagnostics_include_only_metadata(self):
        entry = dict(self.entry, installer_path="app/LeapSDK/include/LeapC.h")
        text = FETCH.verification_failure(self.destination, entry)
        self.assertIn("actual missing", text)
        self.assertIn("extracted_file_count=0", text)
        self.assertNotIn(self.payload.decode(), text)

    def test_wrong_extracted_bytes_report_actual_size_and_hash(self):
        entry = dict(self.entry, installer_path="app/LeapSDK/include/LeapC.h")
        actual = FETCH.safe_path(self.destination, entry["installer_path"])
        actual.parent.mkdir(parents=True)
        actual.write_bytes(b"incorrect bytes")
        text = FETCH.verification_failure(self.destination, entry)
        self.assertIn("actual bytes=15", text)
        self.assertIn(hashlib.sha256(b"incorrect bytes").hexdigest(), text)
        self.assertIn("extracted_file_count=1", text)
        self.assertNotIn("incorrect bytes", text)


if __name__ == "__main__":
    unittest.main()
