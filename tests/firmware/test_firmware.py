#!/usr/bin/env python3
"""Check initramfs firmware inventory and extracted probe error handling.

Run with Python 3 and a C compiler. Pass --modules BUILD_DIR to also check the
firmware metadata in built modules using modinfo. No modules are installed.
The probe test mocks USB and allocation, not kernel scheduling or hardware.
"""
from pathlib import Path
import argparse
import os
import re
import subprocess
import tempfile
import unittest

parser = argparse.ArgumentParser()
parser.add_argument("--modules", type=Path)
args, remaining = parser.parse_known_args()
ROOT = Path(__file__).resolve().parents[2]


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


class FirmwareTests(unittest.TestCase):
    def test_declared_files_match_installed_inventory(self):
        expected = {p.relative_to(ROOT / "fw").as_posix()
                    for p in (ROOT / "fw").rglob("*") if p.is_file()}
        header = (ROOT / "drivers/aic8800/aicwf_firmware.h").read_text()
        declared = re.findall(r'MODULE_FIRMWARE\("([^"]+)"\)', header)
        self.assertEqual(len(declared), len(set(declared)))
        self.assertEqual(set(declared), expected)

    @unittest.skipUnless(args.modules, "pass --modules to inspect built modules")
    def test_built_modules_declare_installed_files(self):
        expected = {p.relative_to(ROOT / "fw").as_posix()
                    for p in (ROOT / "fw").rglob("*") if p.is_file()}
        for name in ("aic_load_fw", "aic8800_fdrv"):
            with self.subTest(module=name):
                module = args.modules / name / (name + ".ko")
                declared = subprocess.check_output(
                    ["modinfo", "-F", "firmware", str(module)], text=True).splitlines()
                self.assertEqual(set(declared), expected)

    def test_probe_errors_and_intentional_reenumeration(self):
        source = (ROOT / "drivers/aic8800/aic_load_fw/aicwf_usb.c").read_text()
        probe = function(source, "static int aicwf_usb_probe(")
        # The real post-allocation probe and cleanup, with allocations supplied
        # by the fixture, exercises the actual failure labels and return value.
        body = probe[probe.index("ret = aicwf_bus_start(bus_if);"):probe.rfind("}")]
        prelude = r'''
#include <assert.h>
#include <stdbool.h>
#include <errno.h>
#include <stddef.h>
#define USB_SPEED_HIGH 2
#define USB_SPEED_SUPER 3
#define PRODUCT_ID_AIC8801 1
#define PRODUCT_ID_AIC8800D81 2
#define PRODUCT_ID_AIC8800D81X2 3
#define PRODUCT_ID_AIC8800D89X2 4
#define usb_err(...) ((void)0)
#define printk(...) ((void)0)
#define kfree(...) ((void)0)
struct usb_device { int speed; };
struct aic_usb_dev { int chipid; bool app_cmp; void *rx_priv; };
static int config_result, download_result, cleanup, downloads, reboots, fw_loaded;
static struct aic_usb_dev device;
static struct usb_device usb_device;
static int aicwf_bus_start(void *bus) { return 0; }
static void aic_bt_platform_init(struct aic_usb_dev *dev) {}
static void system_reboot(struct aic_usb_dev *dev) { reboots++; }
static void rwnx_send_reboot(struct aic_usb_dev *dev) { reboots++; }
static int system_config(struct aic_usb_dev *dev) { return config_result; }
static int aicfw_download_fw(struct aic_usb_dev *dev) { downloads++; return download_result; }
static void aicwf_bus_deinit(void *dev) { cleanup++; }
static void aicwf_rx_deinit(void *dev) {}
static void aicwf_usb_deinit(void *dev) {}
static void usb_set_intfdata(void *intf, void *data) {}
static int probe(void) {
    int ret = 0;
    struct aic_usb_dev *usb_dev = &device;
    struct usb_device *usb = &usb_device;
    void *bus_if = NULL, *dev = NULL, *intf = NULL;
'''
        checks = r'''
}
static void setup(void) {
    config_result = download_result = cleanup = downloads = reboots = fw_loaded = 0;
    device.chipid = 0; device.app_cmp = false; usb_device.speed = USB_SPEED_HIGH;
}
int main(void) {
    setup(); assert(probe() == 0 && device.app_cmp && fw_loaded && downloads == 1);
    setup(); config_result = -EIO;
    assert(probe() == -EIO && cleanup == 1 && downloads == 0 && !fw_loaded);
    setup(); config_result = 1;
    assert(probe() == -EIO && cleanup == 1 && downloads == 0);
    setup(); download_result = -ENOMEM;
    assert(probe() == -ENOMEM && cleanup == 1 && !device.app_cmp && !fw_loaded);
    setup(); download_result = 1; assert(probe() == -EIO && cleanup == 1);
    setup(); device.chipid = PRODUCT_ID_AIC8800D81;
    assert(probe() == 0 && reboots == 1 && cleanup == 1 && downloads == 0);
    setup(); usb_device.speed = 1;
    assert(probe() == 0 && reboots == 1 && cleanup == 1 && downloads == 0);
    return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix="aic-firmware-test-") as temp:
            c_file = Path(temp) / "probe.c"
            binary = Path(temp) / "probe"
            c_file.write_text(prelude + body + checks)
            subprocess.run([os.environ.get("CC", "cc"), "-std=gnu11", "-o",
                            str(binary), str(c_file)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main(argv=[__file__] + remaining)

