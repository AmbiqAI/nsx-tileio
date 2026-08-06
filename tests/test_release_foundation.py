"""Fast host-side contract tests for the release foundation.

These tests intentionally avoid NSX/AmbiqSuite dependencies. Target builds are
separate consumer tests because the SDK supplies the board and toolchain graph.
"""

from __future__ import annotations

import re
import subprocess
import io
import tarfile
import unittest
from pathlib import Path


ROOT = Path(__file__).parents[1]
USB = ROOT / "modules/nsx-tileio-usb"
BLE = ROOT / "modules/nsx-tileio-ble"


def metadata_version(path: Path) -> str:
    text = path.read_text()
    match = re.search(r'(?m)^  version:\s*"([^"]+)"\s*$', text)
    if not match:
        raise AssertionError(f"missing module version in {path}")
    return match.group(1)


class ReleaseMetadataTests(unittest.TestCase):
    def test_project_and_module_versions_are_coherent_semver(self) -> None:
        version = (ROOT / "version.txt").read_text().strip()
        self.assertRegex(version, r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$")
        self.assertEqual(version, metadata_version(USB / "nsx-module.yaml"))
        self.assertEqual(version, metadata_version(BLE / "nsx-module.yaml"))
        self.assertIn(f"VERSION {version}", (ROOT / "CMakeLists.txt").read_text())

    def test_release_foundation_and_license_files_exist(self) -> None:
        for name in (
            "CHANGELOG.md",
            "LICENSE",
            "NOTICE",
            "OWNERS.md",
            "PROVENANCE.md",
            "RELEASE.md",
            "version.txt",
        ):
            self.assertTrue((ROOT / name).is_file(), name)
        self.assertIn("BSD 3-Clause License", (ROOT / "LICENSE").read_text())
        self.assertIn("AmbiqAI/ns-tileio", (ROOT / "PROVENANCE.md").read_text())


class ModuleContractTests(unittest.TestCase):
    def test_usb_target_and_dependencies(self) -> None:
        cmake = (USB / "CMakeLists.txt").read_text()
        metadata = (USB / "nsx-module.yaml").read_text()
        self.assertIn("add_library(nsx::tileio_usb ALIAS nsx_tileio_usb)", cmake)
        self.assertIn("nsx::core", cmake)
        self.assertIn("nsx::usb", cmake)
        self.assertIn("- nsx-core", metadata)
        self.assertIn("- nsx-usb", metadata)

    def test_ble_target_and_dependencies(self) -> None:
        cmake = (BLE / "CMakeLists.txt").read_text()
        metadata = (BLE / "nsx-module.yaml").read_text()
        self.assertIn("add_library(nsx::tileio_ble ALIAS nsx_tileio_ble)", cmake)
        self.assertIn("nsx::core", cmake)
        self.assertIn("nsx::ble", cmake)
        self.assertIn("- nsx-core", metadata)
        self.assertIn("- nsx-ble", metadata)


class UsbFramingTests(unittest.TestCase):
    def setUp(self) -> None:
        self.source = (USB / "src/tio_usb.c").read_text()
        self.header = (USB / "includes-api/tio_usb.h").read_text()

    def test_fixed_frame_and_uio_request_contract(self) -> None:
        self.assertIn("#define TIO_USB_PACKET_LEN 256u", self.header)
        self.assertIn("data_len != 0u && data_len != TIO_USB_UIO_BUF_LEN", self.source)
        self.assertIn("tio_usb_send_uio_state", self.header)
        self.assertIn("slot_type > 2u", self.source)

    def test_partial_send_guard_precedes_vendor_send(self) -> None:
        guard = "nsx_usb_vendor_write_available(&g_tio_usb_cfg) < length"
        self.assertIn(guard, self.source)
        self.assertLess(
            self.source.index(guard),
            self.source.index("uint32_t status = nsx_usb_vendor_send("),
        )
        self.assertIn("NSX_USB_STATUS_BUSY", self.source)

    def test_timed_packet_api_is_absent(self) -> None:
        public = self.header + (USB / "README.md").read_text()
        self.assertNotRegex(public, r"(?i)timed[_ -]?packet")
        self.assertNotIn("slot_type > 3u", self.source)


class ReleaseAutomationTests(unittest.TestCase):
    def test_workflows_pin_actions_and_archive_checksum(self) -> None:
        ci = (ROOT / ".github/workflows/ci.yml").read_text()
        release = (ROOT / ".github/workflows/release.yml").read_text()
        for workflow in (ci, release):
            self.assertRegex(workflow, r"actions/checkout@[0-9a-f]{40}")
        self.assertRegex(release, r"softprops/action-gh-release@[0-9a-f]{40}")
        self.assertIn("git archive --format=tar.gz", release)
        self.assertIn("sha256sum", release)
        self.assertIn("git tag -a", release)

    def test_source_archive_contains_release_contract(self) -> None:
        if not (ROOT / ".github/workflows/release.yml").is_file():
            self.fail("release workflow missing")
        files = subprocess.check_output(
            ["git", "ls-files"], cwd=ROOT, text=True
        ).splitlines()
        required = {"LICENSE", "NOTICE", "PROVENANCE.md", "version.txt"}
        tracked_or_present = set(files) | {
            path.name
            for path in ROOT.iterdir()
            if path.is_file() and ".git" not in path.parts
        }
        self.assertTrue(required.issubset(tracked_or_present), required - tracked_or_present)
        if required.issubset(
            subprocess.check_output(["git", "ls-tree", "-r", "--name-only", "HEAD"],
                                    cwd=ROOT, text=True).splitlines()
        ):
            archive = subprocess.check_output(
                ["git", "archive", "--format=tar", "HEAD"], cwd=ROOT
            )
            with tarfile.open(fileobj=io.BytesIO(archive), mode="r:") as tar:
                names = {Path(member.name).name for member in tar.getmembers()}
        else:
            # Keep the test useful before the first release-foundation commit.
            names = {
                path.name
                for path in ROOT.rglob("*")
                if path.is_file() and ".git" not in path.parts
            }
        self.assertTrue(required.issubset(names), required - names)
        self.assertFalse(any(name == ".git" for name in names))


if __name__ == "__main__":
    unittest.main()
