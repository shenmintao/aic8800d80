# Issue #63: standard `btusb` ACL ZLP support

The `aic_zlp_quirk` companion module supplies the Bluetooth ACL bulk TX
zero-length-packet behavior confirmed in
[issue #63](https://github.com/shenmintao/aic8800d80/issues/63). It is now part
of the normal `aic8800` DKMS build rather than a replacement for the
distribution's Bluetooth driver.

The validated failure was specific and reproducible: AAC playback through USB
device `368b:8d81` stalled after about one minute, while SBC remained stable.
Adding `URB_ZERO_PACKET` to the ACL bulk OUT URBs kept AAC connected for more
than one hour. The standalone module was then verified with the system
`btusb`, no loaded `aic_btusb`, and 2,113 observed ZLP injections.

## Architecture and scope

The module first tries to attach a kretprobe to the standard `btusb` function
that allocates ACL bulk OUT URBs. The target is module-qualified as
`btusb:alloc_bulk_urb`, avoiding similarly named symbols in unrelated USB
drivers.

Some distribution builds inline that private function. When the preferred hook
is unavailable, the module falls back to the stable `usb_submit_urb` entry
point. The fallback changes an URB only when all of these checks pass:

- USB device is exactly `368b:8d81`;
- the transfer is bulk OUT;
- the endpoint belongs to Bluetooth interface 0 (`e0/01/01`).

Both paths add only `URB_ZERO_PACKET`. Wi-Fi interfaces and unrelated USB
devices are unchanged. If neither probe can be installed or kprobes are
disabled, the module refuses to load and system `btusb` remains unchanged.

The module carries a USB modalias for `368b:8d81`, so it is inactive on other
hardware and normally autoloads only when the validated device appears.

## Install from the unified branch

```bash
git fetch origin
git switch test/unified-wifi-bt-zlp
git pull --ff-only
sudo ./install.sh
sudo reboot
```

The installer removes the earlier `btusb-aic-zlp/0.1` and
`aic-zlp-quirk/0.1` diagnostic DKMS packages when present. To remove them
manually before installation, use:

```bash
sudo dkms remove btusb-aic-zlp/0.1 --all
sudo dkms remove aic-zlp-quirk/0.1 --all
sudo depmod -a
```

The unified installer also removes active configuration left by the obsolete
custom `aic_btusb` transport.

## Verify

Confirm that system `btusb` owns Bluetooth interfaces 0/1 and that the quirk is
loaded:

```bash
lsusb -t
lsmod | grep -E '^(btusb|aic_zlp_quirk|aic_btusb)\b'
modinfo -n btusb
modinfo -n aic_zlp_quirk
cat /sys/module/aic_zlp_quirk/parameters/hook
cat /sys/module/aic_zlp_quirk/parameters/injections
sudo ./diagnose_bt.sh
```

Expected results include:

```text
Bluetooth interfaces 0/1: btusb
aic_btusb: not loaded
aic_zlp_quirk: loaded
active hook: btusb:alloc_bulk_urb
```

`active hook: usb_submit_urb` is also valid when the compiler inlined the
preferred function. During Bluetooth traffic, the `injections` value must
increase.

For the promotion test, select AAC and play audio for at least one hour while
also confirming that Wi-Fi scan, association, DHCP, and real traffic remain
working.

## Isolated retest helper

`aic-zlp-quirk-test.sh` remains available only for isolated issue #63 retests.
It builds the same canonical source from
`drivers/aic8800/aic_zlp_quirk/aic_zlp_quirk.c` as a separate DKMS package. A
normal installation should use the repository-level `install.sh` instead.

## Kernel requirements

- matching kernel headers;
- `CONFIG_KPROBES=y` and kretprobe support;
- standard `btusb` plus either its `alloc_bulk_urb` symbol or the USB core
  `usb_submit_urb` symbol;
- a valid module signature when Secure Boot policy requires one.

The preferred function has the same signature in Linux 5.15, 6.1, 6.6, 6.12,
6.18, and 7.1.3, although compilers may inline it. DKMS builds one module for
each installed kernel from the single canonical source file.
