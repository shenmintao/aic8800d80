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
        names = ["run_logged", "cleanup_on_error", "firmware_path_owner", "firmware_matches_shipped", name]
        if name == "main" and re.search(r"(?m)^preflight_build\(\)", SOURCE):
            names.insert(-1, "preflight_build")
        if name == "install_kernel_headers" and re.search(r"(?m)^kernel_headers_metapackages\(\)", SOURCE):
            names.insert(-1, "kernel_headers_metapackages")
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
SRC_DIR="$SCRIPT_DIR/installed-source"
TMPDIR="$SCRIPT_DIR/tmp"
export TMPDIR
mkdir -p "$TMPDIR"
FW_BACKUP_ROOT="$SCRIPT_DIR/backups"
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

    def test_secure_boot_without_terminal_stops_with_failure(self):
        # stdin is not a terminal here, so nobody can answer the prompt. The
        # installer must stop with a non-zero status (not report success) and
        # must not continue to the next step.
        result = self.run_installer_function(
            "check_secure_boot", 'mokutil() { echo "SecureBoot enabled"; }', "\necho UNEXPECTED_CONTINUE",
        )
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("no terminal is attached", result.stdout)
        self.assertIn("mokutil --import", result.stdout)
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
        # The old firmware is moved to a backup, not deleted.
        backups = list((self.root / "backups").glob("aic8800-firmware-*/aic8800D80/old.bin"))
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_bytes(), b"old")

    def test_firmware_identical_to_the_shipped_copy_is_not_backed_up(self):
        installed = self.firmware / "aic8800D80" / "new.bin"
        installed.parent.mkdir()
        installed.write_bytes(b"new")
        shipped = self.root / "fw" / "aic8800D80" / "new.bin"
        shipped.parent.mkdir(parents=True)
        shipped.write_bytes(b"new")
        result = self.run_installer_function("install_firmware")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertFalse((self.root / "backups").exists())
        self.assertEqual(installed.read_bytes(), b"new")
        self.assertFalse((installed.parent / "aic8800D80").exists())

    def test_changed_or_extra_firmware_is_still_backed_up(self):
        installed = self.firmware / "aic8800D80" / "new.bin"
        installed.parent.mkdir()
        installed.write_bytes(b"edited by hand")
        shipped = self.root / "fw" / "aic8800D80" / "new.bin"
        shipped.parent.mkdir(parents=True)
        shipped.write_bytes(b"new")
        result = self.run_installer_function("install_firmware")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        backups = list((self.root / "backups").glob("aic8800-firmware-*/aic8800D80/new.bin"))
        self.assertEqual([b.read_bytes() for b in backups], [b"edited by hand"])

    def run_headers(self, release, installed, build_tree=True):
        """installed: (dpkg status, package) pairs that dpkg-query reports."""
        listing = "\n".join(f"{status} {package}" for status, package in installed)
        available = ("linux-headers-generic", "linux-headers-generic-hwe-24.04", "linux-headers-amd64")
        stubs = f"""
uname() {{ echo {shlex.quote(release)}; }}
dpkg-query() {{ printf '%s\\n' {shlex.quote(listing)}; [ -n {shlex.quote(listing)} ]; }}
apt-cache() {{ case "$2" in {"|".join(available)}) return 0 ;; esac; return 100; }}
run_logged() {{ shift; echo "RUN: $*"; }}
kernel_build_tree_available() {{ return {0 if build_tree else 1}; }}
"""
        result = self.run_installer_function("install_kernel_headers", stubs=stubs, after=" apt")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        runs = [line[5:] for line in result.stdout.splitlines() if line.startswith("RUN: ")]
        return result, runs

    APT = "env DEBIAN_FRONTEND=noninteractive apt-get install -y "

    def test_headers_follow_the_ga_track(self):
        _, runs = self.run_headers("6.8.0-149-generic", [
            ("ii", "linux-image-6.8.0-149-generic"), ("ii", "linux-image-generic")])
        self.assertEqual(runs, [self.APT + "linux-headers-generic"])

    def test_headers_follow_the_hwe_track(self):
        # GA and HWE kernels both end in "-generic"; only the metapackage differs.
        _, runs = self.run_headers("7.0.0-42-generic", [
            ("ii", "linux-image-7.0.0-42-generic"), ("ii", "linux-image-generic-hwe-24.04")])
        self.assertEqual(runs, [self.APT + "linux-headers-generic-hwe-24.04"])

    def test_headers_for_every_installed_track(self):
        _, runs = self.run_headers("6.8.0-149-generic", [
            ("ii", "linux-image-generic"), ("ii", "linux-image-generic-hwe-24.04")])
        self.assertEqual(runs, [self.APT + "linux-headers-generic linux-headers-generic-hwe-24.04"])

    def test_headers_debian_metapackage(self):
        _, runs = self.run_headers("6.12.115+deb13.1-amd64", [
            ("ii", "linux-image-6.12.115+deb13.1-amd64"), ("ii", "linux-image-amd64")])
        self.assertEqual(runs, [self.APT + "linux-headers-amd64"])

    def test_headers_for_running_kernel_when_build_tree_missing(self):
        _, runs = self.run_headers("7.0.0-42-generic", [("ii", "linux-image-generic-hwe-24.04")], build_tree=False)
        self.assertEqual(runs, [self.APT + "linux-headers-7.0.0-42-generic linux-headers-generic-hwe-24.04"])

    def test_headers_skip_removed_unsigned_and_debug_packages(self):
        result, runs = self.run_headers("6.12.115+deb13.1-amd64", [
            ("rc", "linux-image-amd64"), ("ii", "linux-image-unsigned-6.8.0-47-generic"),
            ("ii", "linux-image-amd64-dbg"), ("ii", "linux-image-6.12.115+deb13.1-amd64")])
        self.assertEqual(runs, [])
        self.assertIn("No kernel headers metapackage", result.stdout)

    def prepare_changed_firmware(self):
        installed = self.firmware / "aic8800D80" / "new.bin"
        installed.parent.mkdir()
        installed.write_bytes(b"edited by hand")
        shipped = self.root / "fw" / "aic8800D80" / "new.bin"
        shipped.parent.mkdir(parents=True)
        shipped.write_bytes(b"new")

    def installer_backups(self):
        return sorted(d.name for d in (self.root / "backups").glob("aic8800-firmware-install-*") if d.is_dir())

    def test_firmware_backup_reports_where_to_restore_from(self):
        self.prepare_changed_firmware()
        result = self.run_installer_function("install_firmware")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        backups = self.installer_backups()
        self.assertEqual(len(backups), 1)
        self.assertIn(f"Previous firmware saved in {self.root}/backups/{backups[0]}", result.stdout)
        self.assertIn("To restore it", result.stdout)

    def test_only_the_three_newest_installer_backups_are_kept(self):
        for day in (1, 2, 3, 4):
            (self.root / "backups" / f"aic8800-firmware-install-2026010{day}-000000" / "aic8800D80").mkdir(parents=True)
        # Not provably made by this installer: uninstall.sh's and older installers'.
        others = [self.root / "backups" / name for name in
                  ("aic8800-firmware-20260101-000000", "aic8800-firmware-20250101-000000")]
        for other in others:
            other.mkdir(parents=True)
        self.prepare_changed_firmware()
        result = self.run_installer_function("install_firmware")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        backups = self.installer_backups()
        self.assertEqual(len(backups), 3, backups)
        self.assertEqual(backups[:2], ["aic8800-firmware-install-20260103-000000",
                                       "aic8800-firmware-install-20260104-000000"])
        self.assertEqual((self.root / "backups" / backups[2] / "aic8800D80" / "new.bin").read_bytes(), b"edited by hand")
        for other in others:
            self.assertTrue(other.exists(), other)

    def test_new_backup_is_kept_even_if_older_names_sort_after_it(self):
        for month in ("01", "02", "03"):
            (self.root / "backups" / f"aic8800-firmware-install-2099{month}01-000000").mkdir(parents=True)
        self.prepare_changed_firmware()
        result = self.run_installer_function("install_firmware")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        backups = self.installer_backups()
        self.assertEqual(len(backups), 3, backups)
        self.assertNotIn("aic8800-firmware-install-20990101-000000", backups)
        self.assertEqual(len([n for n in backups if "-2099" not in n]), 1, backups)

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
require_dkms_conf() { :; }
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

    def test_missing_dkms_conf_stops_before_any_change(self):
        result = self.run_installer_function("require_dkms_conf")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("dkms.conf not found", result.stdout)

    def test_shipped_dkms_conf_is_accepted(self):
        (self.root / "dkms.conf").write_text("PACKAGE_NAME=aic8800\n", encoding="utf-8")
        result = self.run_installer_function("require_dkms_conf")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

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
