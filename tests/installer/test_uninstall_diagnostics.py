#!/usr/bin/env python3
"""Regressions for uninstall.sh and the diagnostic output filter.

Usage: python3 tests/installer/test_uninstall_diagnostics.py
Runs as a normal user. uninstall.sh runs against a temporary DESTDIR, with
dkms, modprobe, udevadm, the initramfs tools and the package managers
replaced by stubs, so nothing on the system is changed.
"""

from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile
import textwrap
import unittest

REPO = Path(__file__).resolve().parents[2]
UNINSTALLER = REPO / "uninstall.sh"
DIAGNOSTIC_SCRIPTS = [REPO / "diagnose_bt.sh", REPO / "diagnostic_build.sh"]
STUBBED = ["dkms", "modprobe", "udevadm", "update-initramfs", "dracut", "mkinitcpio",
           "dpkg", "rpm", "pacman"]
REGISTERED = "aic8800/1.0.0, 6.12.113+deb13-amd64, x86_64: installed"


class UninstallTests(unittest.TestCase):
    def setUp(self):
        if os.geteuid() == 0:
            self.fail("run as a normal user, not root")
        temp = tempfile.TemporaryDirectory(prefix="aic-uninstall-test-")
        self.addCleanup(temp.cleanup)
        base = Path(temp.name)
        self.root = base / "root"
        self.stubs = base / "stubs"
        self.calls = base / "calls.log"
        self.root.mkdir()
        self.stubs.mkdir()
        self.calls.touch()
        for name in STUBBED:
            self.stub(name, "exit 0")
        self.stub("dpkg", "exit 1")  # no package owns the firmware
        self.write("usr/src/aic8800-1.0.0/dkms.conf", 'PACKAGE_NAME="aic8800"\n')
        self.rules = self.write("etc/udev/rules.d/aic.rules", "# AIC8800 rules\n")
        self.firmware = self.write("lib/firmware/aic8800D80/fw.bin", "fw")

    def stub(self, name, body):
        path = self.stubs / name
        path.write_text("#!/bin/bash\n"
                        f'printf "%s\\n" "{name} $*" >> {shlex.quote(str(self.calls))}\n'
                        + textwrap.dedent(body) + "\n", encoding="utf-8")
        path.chmod(0o755)

    def write(self, relative, content):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
        return path

    def source(self, version):
        return self.root / f"usr/src/aic8800-{version}/dkms.conf"

    def uninstall(self):
        env = {"PATH": f"{self.stubs}:{os.environ['PATH']}", "HOME": str(self.root),
               "DESTDIR": str(self.root), "LANG": "C"}
        return subprocess.run(["bash", str(UNINSTALLER)], text=True, capture_output=True,
                              timeout=60, env=env)

    def test_successful_removal_deletes_source_and_succeeds(self):
        self.stub("dkms", f'[ "$1" = status ] && echo "{REGISTERED}"\nexit 0')
        result = self.uninstall()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertFalse(self.source("1.0.0").exists())

    def test_failed_removal_keeps_source_and_fails(self):
        self.stub("dkms", f'[ "$1" = status ] && echo "{REGISTERED}"\n[ "$1" = remove ] && exit 1\nexit 0')
        result = self.uninstall()
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(self.source("1.0.0").exists())
        self.assertIn("keeping", result.stdout)
        # The other steps still run.
        self.assertFalse(self.rules.exists())
        self.assertFalse(self.firmware.exists())

    def test_only_the_failed_version_keeps_its_source(self):
        self.write("usr/src/aic8800-1.0.1/dkms.conf", 'PACKAGE_NAME="aic8800"\n')
        self.stub("dkms", f'[ "$1" = status ] && printf "%s\\\\n" "{REGISTERED}" '
                          '"aic8800/1.0.1, 6.12.115+deb13-amd64, x86_64: installed"\n'
                          '[ "$1" = remove ] && [ "$2" = aic8800/1.0.1 ] && exit 1\nexit 0')
        result = self.uninstall()
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertFalse(self.source("1.0.0").exists())
        self.assertTrue(self.source("1.0.1").exists())

    def test_failed_dkms_status_keeps_every_source(self):
        self.stub("dkms", '[ "$1" = status ] && exit 3\nexit 0')
        result = self.uninstall()
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(self.source("1.0.0").exists())
        self.assertNotIn("dkms remove", self.calls.read_text())

    def test_package_owned_rules_and_modeswitch_config_are_kept(self):
        # A marker string alone does not make a file this project's to delete.
        # The AUR package aic8800d80-dkms installs /usr/lib/udev/rules.d/aic.rules.
        packaged = self.write("usr/lib/udev/rules.d/aic.rules", "# AIC8800 rules from a package\n")
        modeswitch = self.write("etc/usb_modeswitch.d/1111:1111", "# AIC8800D80\n")
        self.stub("dkms", f'[ "$1" = status ] && echo "{REGISTERED}"\nexit 0')
        self.stub("dpkg", '[ "$1" = -S ] || exit 1\n'
                          'case "$2" in /usr/lib/udev/rules.d/aic.rules|/etc/usb_modeswitch.d/1111:1111)\n'
                          '  echo "aic8800-dkms: $2"; exit 0 ;;\nesac\nexit 1')
        result = self.uninstall()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(packaged.read_text(), "# AIC8800 rules from a package\n")
        self.assertTrue(modeswitch.exists())
        self.assertIn("owned by package 'aic8800-dkms'", result.stdout)
        # This project's own, unowned copy is still removed.
        self.assertFalse(self.rules.exists())


def sanitize(script, text):
    match = re.search(r"(?ms)^sanitize_output\(\) \{.*?^\}", script.read_text(encoding="utf-8"))
    if not match:
        raise AssertionError(f"sanitize_output not found in {script}")
    program = f"hostname() {{ echo workstation; }}\n{match.group()}\nsanitize_output"
    env = {"PATH": "/usr/bin:/bin", "SUDO_USER": "alice", "AIC_DIAG_RAW": "0"}
    return subprocess.run(["bash", "-c", program], input=text + "\n", text=True,
                          capture_output=True, env=env, check=True).stdout.strip()


class SanitizerTests(unittest.TestCase):
    def test_keeps_driver_and_firmware_versions(self):
        # RWNX_VERS_BANNER, and the firmware version string the driver logs at
        # LOGINFO. Every firmware file carries a "v<a>.<b>.<c>.<d>" string.
        for line in ("rwnx v6.4.3.0 - 1a4b0054d2M (master)",
                     "AICWFDBG(LOGINFO)\tversion_str=v6.9.1.1"):
            for script in DIAGNOSTIC_SCRIPTS:
                with self.subTest(script=script.name, line=line):
                    self.assertEqual(sanitize(script, line), line)

    def test_masks_addresses(self):
        # Kernel log formats (NFS, CIFS) plus the ip(8) form; RFC 5737 addresses.
        # The space-separated MAC is aic8800_fdrv's p2p line.
        cases = {
            "inet 192.0.2.10/24": "inet <IP>/24",
            "nfs: server 192.0.2.20 not responding, still trying":
                "nfs: server <IP> not responding, still trying",
            "CIFS: VFS: \\\\198.51.100.30 Send error in SessSetup = -13":
                "CIFS: VFS: \\\\<IP> Send error in SessSetup = -13",
            "AICWFDBG(LOGERROR)\tp2p dev addr=2 11 22 33 44 55": "AICWFDBG(LOGERROR)\tp2p dev addr=<MAC>",
        }
        for script in DIAGNOSTIC_SCRIPTS:
            for line, expected in cases.items():
                with self.subTest(script=script.name, line=line):
                    self.assertEqual(sanitize(script, line), expected)

    def test_masks_addresses_in_firewall_log(self):
        line = ("[UFW BLOCK] IN=wlx0123456789ab OUT= MAC=01:00:5e:00:00:fb:02:00:00:aa:bb:cc:08:00 "
                "SRC=203.0.113.5 DST=192.0.2.10 LEN=73 TOS=0x00 PREC=0x00 TTL=255 ID=0 DF "
                "PROTO=UDP SPT=5353 DPT=5353 LEN=53")
        for script in DIAGNOSTIC_SCRIPTS:
            with self.subTest(script=script.name):
                out = sanitize(script, line)
                self.assertIn("SRC=<IP> DST=<IP>", out)
                for leaked in ("203.0.113.5", "192.0.2.10", "02:00:00:aa:bb:cc", "0123456789ab"):
                    self.assertNotIn(leaked, out)


if __name__ == "__main__":
    unittest.main(verbosity=2)
