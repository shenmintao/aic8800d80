#!/usr/bin/env python3
"""Check device/event selection using fixtures; does not switch real devices.

This models the basic equality/glob predicates used by aic.rules. Run
udevadm verify separately to validate syntax with the real udev parser.
"""

from fnmatch import fnmatchcase
from pathlib import Path
import re
import unittest


RULES = (Path(__file__).resolve().parents[2] / "aic.rules").read_text().splitlines()


def matching_rules(device):
    result = []
    for rule in RULES:
        if not rule.strip() or rule.lstrip().startswith("#"):
            continue
        predicates = re.findall(r'([A-Z]+(?:\{[^}]+\})?)=="([^"]*)"', rule)
        if predicates and all(fnmatchcase(device.get(key, ""), value) for key, value in predicates):
            result.append(rule)
    return result


class UdevTests(unittest.TestCase):
    def pandora(self, **changes):
        device = {"ACTION": "add", "SUBSYSTEM": "usb", "ENV{DEVTYPE}": "usb_device",
                  "ATTR{idVendor}": "1111", "ATTR{idProduct}": "1111", "ATTR{product}": "88M80"}
        device.update(changes)
        return device

    def test_88m80_uses_event_bus_and_device_numbers(self):
        for product in ("88M80", "88M80 USB WiFi"):
            with self.subTest(product=product):
                matches = matching_rules(self.pandora(**{"ATTR{product}": product}))
                self.assertEqual(len(matches), 1)
                # Both possible usb_modeswitch executable locations target this event's device.
                self.assertEqual(matches[0].count("-b $attr{busnum} -g $attr{devnum}"), 2)

    def test_generic_1111_flash_disk_is_not_switched(self):
        for product in ("FLASH UDISK 2.30", "USB DISK", ""):
            with self.subTest(product=product):
                self.assertEqual(matching_rules(self.pandora(**{"ATTR{product}": product})), [])

    def test_wrong_ids_interfaces_and_non_add_events_are_not_switched(self):
        for changes in ({"ACTION": "change"}, {"ACTION": "remove"},
                        {"ENV{DEVTYPE}": "usb_interface"}, {"ATTR{idVendor}": "2222"},
                        {"ATTR{idProduct}": "8d80"}):
            with self.subTest(changes=changes):
                self.assertEqual(matching_rules(self.pandora(**changes)), [])

    def test_eject_only_on_add_of_a_whole_supported_disk(self):
        for product in ("5721", "5722", "5723", "5724", "5725", "5726", "5727", "572a", "572c", "572f"):
            disk = {"ACTION": "add", "SUBSYSTEM": "block", "ENV{DEVTYPE}": "disk", "KERNEL": "sdb",
                    "ATTRS{idVendor}": "a69c", "ATTRS{idProduct}": product}
            with self.subTest(product=product):
                matches = matching_rules(disk)
                self.assertEqual(len(matches), 1)
                self.assertIn('/usr/bin/eject /dev/%k', matches[0])
                for changes in ({"ACTION": "change"}, {"ACTION": "remove"},
                                {"ENV{DEVTYPE}": "partition", "KERNEL": "sdb1"}):
                    self.assertEqual(matching_rules(dict(disk, **changes)), [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
