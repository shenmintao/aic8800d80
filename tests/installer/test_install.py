#!/usr/bin/env python3
"""Run installer regressions with mocked system commands and temporary firmware.

Usage: python3 tests/installer/test_install.py [path/to/install.sh]
Requires Bash; does not install packages, load modules, or need root.
"""

from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
import unittest


INSTALLER = Path(sys.argv.pop(1)) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / "install.sh"
SOURCE = INSTALLER.read_text(encoding="utf-8")
SHELL_OPTIONS = next(line for line in SOURCE.splitlines() if line.startswith("set -"))


class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="aic-installer-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.firmware = self.root / "firmware"
        self.firmware.mkdir()

    def run_installer_function(self, name, stubs="", after=""):
        # Extract only the functions under test; never execute main().
        names = ["run_logged", "cleanup_on_error", name]
        functions = []
        for function in names:
            match = re.search(r"(?ms)^" + function + r"\(\) \{.*?^\}", SOURCE)
            self.assertIsNotNone(match, function)
            functions.append(match.group())
        script = "\n".join(functions)
        script = script.replace("/lib/firmware", str(self.firmware))
        script = script.replace("/sys/firmware/efi", str(self.root / "efi"))
        setup = f"""
{SHELL_OPTIONS}
SCRIPT_DIR={shlex.quote(str(self.root))}
LOG_FILE="$SCRIPT_DIR/install.log"
DRV_NAME=aic8800
DRV_VERSION=1.0.0
MODULE_NAME=aic8800_fdrv
print_step() {{ echo "$*"; }}
print_info() {{ echo "$*"; }}
print_success() {{ echo "$*"; }}
print_warning() {{ echo "$*"; }}
print_error() {{ echo "$*"; }}
dkms() {{ echo 'aic8800/1.0.0: installed'; }}
lsmod() {{ :; }}
aic_zlp_target_present() {{ return 1; }}
iwconfig() {{ :; }}
bluetoothctl() {{ :; }}
kernel_build_tree_available() {{ return 0; }}
is_volumio() {{ return 1; }}
cleanup_legacy_bluetooth_config() {{ echo LEGACY_CLEANUP; }}
"""
        return subprocess.run(
            ["bash", "-c", setup + script + "\ntrap cleanup_on_error ERR\n" + stubs + "\n" + name + after],
            input="", text=True, capture_output=True, timeout=15,
        )

    def test_verification_reaches_final_instructions(self):
        for count in (0, 1, 3):
            with self.subTest(firmware_count=count):
                for index in range(count):
                    (self.firmware / f"aic8800-{index}").mkdir(exist_ok=True)
                result = self.run_installer_function("verify_installation", after="\necho FINAL_INSTRUCTIONS")
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("FINAL_INSTRUCTIONS", result.stdout)
                if count:
                    self.assertIn(f"Firmware installed for {count} chip variant(s)", result.stdout)

    def test_pacman_uses_existing_database(self):
        result = self.run_installer_function(
            "install_dependencies", 'pacman() { echo "PACMAN $*"; }', " pacman",
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        calls = [line for line in result.stdout.splitlines() if line.startswith("PACMAN ")]
        self.assertEqual(calls, ["PACMAN -S --needed --noconfirm dkms base-devel mokutil util-linux usb_modeswitch"])

    def test_pacman_failure_stops_with_upgrade_guidance(self):
        result = self.run_installer_function(
            "install_dependencies", 'pacman() { return 7; }', " pacman\necho UNEXPECTED_SUCCESS",
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("sudo pacman -Syu", result.stdout)
        self.assertNotIn("UNEXPECTED_SUCCESS", result.stdout)
        self.assertIn("Installation failed", result.stdout)

    def test_secure_boot_eof_cancels_cleanly(self):
        result = self.run_installer_function(
            "check_secure_boot", 'mokutil() { echo "SecureBoot enabled"; }', "\necho UNEXPECTED_CONTINUE",
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Installation cancelled", result.stdout)
        self.assertNotIn("UNEXPECTED_CONTINUE", result.stdout)

    def check_invalid_firmware(self):
        old = self.firmware / "aic8800D80" / "old.bin"
        old.parent.mkdir()
        old.write_bytes(b"installed firmware")
        result = self.run_installer_function("install_firmware")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(old.read_bytes(), b"installed firmware")
        self.assertNotIn("LEGACY_CLEANUP", result.stdout)

    def test_missing_firmware_preserves_installation(self):
        self.check_invalid_firmware()

    def test_empty_firmware_tree_preserves_installation(self):
        (self.root / "fw").mkdir()
        self.check_invalid_firmware()

    def test_empty_variant_preserves_installation(self):
        (self.root / "fw" / "aic8800D80").mkdir(parents=True)
        self.check_invalid_firmware()

    def test_valid_firmware_replaces_old_variant(self):
        old = self.firmware / "aic8800D80" / "old.bin"
        old.parent.mkdir()
        old.write_bytes(b"old")
        new = self.root / "fw" / "aic8800D80" / "new.bin"
        new.parent.mkdir(parents=True)
        new.write_bytes(b"new")
        result = self.run_installer_function("install_firmware")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertFalse(old.exists())
        self.assertEqual((old.parent / "new.bin").read_bytes(), b"new")


if __name__ == "__main__":
    unittest.main(verbosity=2)
