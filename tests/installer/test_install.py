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
        if name == "main" and re.search(r"(?m)^preflight_build\(\)", SOURCE):
            names.insert(-1, "preflight_build")
        functions = []
        for function in names:
            match = re.search(r"(?ms)^" + function + r"\(\) \{.*?^\}", SOURCE)
            self.assertIsNotNone(match, function)
            functions.append(match.group())
        script = "\n".join(functions)
        script = script.replace("/lib/firmware", str(self.firmware))
        script = script.replace("/sys/firmware/efi", str(self.root / "efi"))
        # Keep the udev rules directories inside the temporary root too.
        script = script.replace("/etc/udev/rules.d", str(self.root / "etc/udev/rules.d"))
        script = script.replace("/usr/lib/udev/rules.d", str(self.root / "usr/lib/udev/rules.d"))
        script = re.sub(r"(?<![\w/])/lib/udev/rules\.d", str(self.root / "lib/udev/rules.d"), script)
        setup = f"""
{SHELL_OPTIONS}
SCRIPT_DIR={shlex.quote(str(self.root))}
LOG_FILE="$SCRIPT_DIR/install.log"
SRC_DIR="$SCRIPT_DIR/installed-source"
TMPDIR="$SCRIPT_DIR/tmp"
export TMPDIR
mkdir -p "$TMPDIR"
DRV_NAME=aic8800
DRV_VERSION=1.0.0
MODULE_NAME=aic8800_fdrv
print_step() {{ echo "$*"; }}
print_info() {{ echo "$*"; }}
print_success() {{ echo "$*"; }}
print_warning() {{ echo "$*"; }}
print_error() {{ echo "$*"; }}
log_message() {{ :; }}
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

    def install_rules(self):
        (self.root / "aic.rules").write_text("# AIC8800 rules\n")
        firmware = self.root / "fw" / "aic8800D80" / "new.bin"
        firmware.parent.mkdir(parents=True)
        firmware.write_bytes(b"new")
        stubs = 'udevadm() { echo "$*" >> "$SCRIPT_DIR/udevadm.calls"; }'
        result = self.run_installer_function("install_firmware", stubs=stubs)
        calls = self.root / "udevadm.calls"
        return result, calls.read_text().splitlines() if calls.exists() else []

    def test_rules_install_to_etc_and_leave_vendor_copies_alone(self):
        # The AUR package aic8800d80-dkms installs /usr/lib/udev/rules.d/aic.rules.
        vendor = self.root / "usr" / "lib" / "udev" / "rules.d" / "aic.rules"
        vendor.parent.mkdir(parents=True)
        vendor.write_text("# AIC8800 rules from a package\n")
        result, _ = self.install_rules()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.root / "etc/udev/rules.d/aic.rules").read_text(), "# AIC8800 rules\n")
        self.assertEqual(vendor.read_text(), "# AIC8800 rules from a package\n")

    def test_rules_trigger_only_1111_devices_with_add(self):
        # The rules match ACTION=="add"; a plain "udevadm trigger" sends "change".
        result, calls = self.install_rules()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        triggers = [call for call in calls if call.startswith("trigger")]
        self.assertEqual(len(triggers), 1, calls)
        for option in ("--action=add", "--subsystem-match=usb",
                       "--attr-match=idVendor=1111", "--attr-match=idProduct=1111"):
            self.assertIn(option, triggers[0])

    def prepare_preflight(self):
        source = self.root / "drivers" / "aic8800" / "source.c"
        source.parent.mkdir(parents=True)
        source.write_bytes(b"source")
        old = self.firmware / "aic8800D80" / "old.bin"
        old.parent.mkdir()
        old.write_bytes(b"installed firmware")
        installed = self.root / "installed-source" / "revision"
        installed.parent.mkdir()
        installed.write_bytes(b"installed source")
        (self.root / "dkms-state").write_bytes(b"installed modules")
        return r'''
uname() { echo test-kernel; }
check_root() { :; }
check_secure_boot() { :; }
detect_package_manager() { DETECTED_PKG_MANAGER=apt; }
install_dependencies() { :; }
check_kernel_build_tree() { :; }
create_dkms_conf() { :; }
refresh_initramfs() { :; }
load_module() { :; }
verify_installation() { :; }
show_final_instructions() { echo FINAL_INSTRUCTIONS; }
make() {
    echo "MAKE $*" >> "$SCRIPT_DIR/events"
    [ "$(cat "$2/source.c")" = source ] || return 99
    case "$*" in
        *' clean') return "${MAKE_CLEAN_STATUS:-0}" ;;
    esac
    echo built > "$2/source.c"
    return "${MAKE_BUILD_STATUS:-0}"
}
install_firmware() {
    echo FIRMWARE >> "$SCRIPT_DIR/events"
    echo changed > "$SCRIPT_DIR/firmware/aic8800D80/old.bin"
}
install_via_dkms() {
    echo DKMS >> "$SCRIPT_DIR/events"
    echo changed > "$SRC_DIR/revision"
    echo changed > "$SCRIPT_DIR/dkms-state"
}
'''

    def assert_installation_preserved(self, result):
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.firmware / "aic8800D80/old.bin").read_bytes(), b"installed firmware")
        self.assertEqual((self.root / "installed-source/revision").read_bytes(), b"installed source")
        self.assertEqual((self.root / "dkms-state").read_bytes(), b"installed modules")
        events = self.root / "events"
        if events.exists():
            self.assertNotIn("FIRMWARE", events.read_text())
            self.assertNotIn("DKMS", events.read_text())
        self.assertNotIn("FINAL_INSTRUCTIONS", result.stdout)
        self.assertEqual(list((self.root / "tmp").iterdir()), [])

    def test_failed_preflight_preserves_firmware_source_and_dkms(self):
        stubs = self.prepare_preflight() + "\nMAKE_BUILD_STATUS=17\n"
        result = self.run_installer_function("main", stubs)
        self.assert_installation_preserved(result)
        self.assertIn("Test build failed", result.stdout)
        self.assertIn("MAKE", (self.root / "events").read_text())
        self.assertEqual((self.root / "drivers/aic8800/source.c").read_bytes(), b"source")

    def test_preflight_copy_failure_preserves_installation(self):
        stubs = self.prepare_preflight() + "\ncp() { return 18; }\n"
        result = self.run_installer_function("main", stubs)
        self.assert_installation_preserved(result)
        self.assertFalse((self.root / "events").exists())

    def test_preflight_clean_failure_preserves_installation(self):
        stubs = self.prepare_preflight() + "\nMAKE_CLEAN_STATUS=19\n"
        result = self.run_installer_function("main", stubs)
        self.assert_installation_preserved(result)
        self.assertEqual(len((self.root / "events").read_text().splitlines()), 1)

    def test_preflight_success_precedes_installation_and_cleans_temp(self):
        result = self.run_installer_function("main", self.prepare_preflight())
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        events = (self.root / "events").read_text().splitlines()
        self.assertEqual(len(events), 4)
        self.assertTrue(events[0].endswith(" clean"), events)
        self.assertTrue(events[1].startswith("MAKE "), events)
        self.assertEqual(events[2:], ["FIRMWARE", "DKMS"])
        for line in events[:2]:
            self.assertIn(str(self.root / "tmp"), line)
            self.assertIn("KVER=test-kernel KDIR=/lib/modules/test-kernel/build", line)
        self.assertEqual((self.root / "drivers/aic8800/source.c").read_bytes(), b"source")
        self.assertEqual(list((self.root / "tmp").iterdir()), [])
        self.assertIn("FINAL_INSTRUCTIONS", result.stdout)

    def test_log_replaces_symlink_without_overwriting_target(self):
        stubs = self.prepare_preflight()
        target = self.root / "keep.log"
        target.write_bytes(b"keep this file")
        (self.root / "install.log").symlink_to(target)
        result = self.run_installer_function("main", stubs)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(target.read_bytes(), b"keep this file")
        log = self.root / "install.log"
        self.assertFalse(log.is_symlink())
        self.assertEqual(log.stat().st_mode & 0o777, 0o644)

    def test_root_check_precedes_log_changes(self):
        stubs = self.prepare_preflight() + "\ncheck_root() { return 20; }\n"
        log = self.root / "install.log"
        log.write_bytes(b"previous log")
        result = self.run_installer_function("main", stubs)
        self.assert_installation_preserved(result)
        self.assertEqual(log.read_bytes(), b"previous log")
        self.assertFalse((self.root / "events").exists())

    def test_log_creation_failure_stops_before_preflight(self):
        stubs = self.prepare_preflight() + "\ninstall() { return 21; }\n"
        result = self.run_installer_function("main", stubs)
        self.assert_installation_preserved(result)
        self.assertFalse((self.root / "events").exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
