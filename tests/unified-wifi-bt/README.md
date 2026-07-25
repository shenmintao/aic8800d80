# Unified Wi-Fi, Bluetooth, and ZLP branch test

This branch tests replacing the separate `main` and `bluetooth` branches with
one driver and installer. The Wi-Fi driver and firmware were already identical
on both branches. This test unifies their installer, migration, diagnostics,
packaging, and documentation, and adds the device-scoped ZLP fix from issue
#63 as a third DKMS module.

The expected architecture is:

- `aic_load_fw` initializes every supported adapter and uploads firmware.
- `aic8800_fdrv` handles Wi-Fi.
- A combo adapter exposes a Bluetooth HCI USB interface after initialization;
  the standard Linux `btusb` driver binds through normal USB device matching.
- A Wi-Fi-only adapter exposes no HCI interface and does not need `btusb`.
- The retired custom `aic_btusb` module is never installed or used.
- USB device `368b:8d81` autoloads `aic_zlp_quirk`, which adds
  `URB_ZERO_PACKET` only to its Bluetooth ACL bulk OUT transfers.
- The distribution's original `btusb.ko` is never replaced.

The installer deliberately does not force-load `btusb` and does not globally
unblock Bluetooth through rfkill.

## Install

```bash
git fetch origin
git switch test/unified-wifi-bt-zlp
git pull --ff-only
sudo ./install.sh
sudo reboot
```

## Test a Wi-Fi-only adapter

Confirm that:

1. The Wi-Fi interface is created.
2. Scanning, association, DHCP, and real traffic work.
3. No `aic_btusb` module is loaded.
4. The absence of `btusb` or an HCI controller is treated as normal.

## Test a Wi-Fi/Bluetooth combo adapter

Confirm that:

1. Wi-Fi scanning, association, DHCP, and real traffic work.
2. `aic_load_fw` uploads the firmware before the HCI interface is used.
3. The Bluetooth USB interface is bound to the standard `btusb` driver.
4. `bluetoothctl` can power on, scan, pair, and exchange real traffic.
5. No `aic_btusb` module, alias, soft dependency, or udev binding rule remains.

## Test the `368b:8d81` ZLP path

In addition to the combo-adapter checks, confirm that:

1. `aic_zlp_quirk` loads automatically and system `btusb` owns interfaces 0/1.
2. `hook` reports `btusb:alloc_bulk_urb` or `usb_submit_urb`.
3. The `injections` counter increases during Bluetooth audio traffic.
4. AAC playback remains connected for at least one hour.
5. Wi-Fi scan, association, and traffic remain unaffected during the test.

## Collect results

```bash
git rev-parse --short HEAD
lsusb
lsusb -t
lsmod | grep -E 'aic|btusb|bluetooth'
iw dev
bluetoothctl list
cat /sys/module/aic_zlp_quirk/parameters/{hook,injections} 2>/dev/null || true
sudo ./diagnose_bt.sh | tee unified-wifi-bt-diagnostic.log
sudo journalctl -k -b --no-pager | tee unified-wifi-bt-kernel.log
```

Please attach both logs and identify whether the tested adapter is Wi-Fi-only
or a combo device.

## Roll back during testing

```bash
git switch main
git pull --ff-only
sudo ./install.sh
sudo reboot
```
