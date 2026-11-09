#!/usr/bin/env python3
"""Regressions for uninstall.sh.

Usage: python3 tests/installer/test_uninstall_diagnostics.py
Runs as a normal user. uninstall.sh runs against a temporary DESTDIR, with
dkms, modprobe, udevadm, the initramfs tools and the package managers
replaced by stubs, so nothing on the system is changed.
"""

from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import textwrap
import unittest

REPO = Path(__file__).resolve().parents[2]
UNINSTALLER = REPO / "uninstall.sh"
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



if __name__ == "__main__":
    unittest.main(verbosity=2)
