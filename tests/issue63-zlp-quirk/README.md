# Issue #63: standard `btusb` ZLP companion-module test

This experiment keeps the normal `aic_load_fw + system btusb` architecture.
It does not install or bind the vendor `aic_btusb` transport, and it does not
replace the distribution's `btusb.ko`.

The underlying ZLP behavior was confirmed in
[issue #63](https://github.com/shenmintao/aic8800d80/issues/63): AAC playback
that previously stalled after about one minute remained stable for more than
one hour with the standard `btusb` ZLP patch, including after the obsolete
`aic_btusb` module was removed.

The small `aic_zlp_quirk.ko` module first tries to attach a kretprobe to the
standard `btusb` function that allocates Bluetooth ACL bulk OUT URBs. The
target is module-qualified as `btusb:alloc_bulk_urb`, avoiding similarly named
symbols in unrelated USB drivers.

Some distribution builds inline that private `btusb` function. When the
preferred hook is unavailable, the same module falls back to the stable
`usb_submit_urb` entry point. The fallback requires all of the following before
changing an URB:

- USB device `368b:8d81`;
- bulk OUT transfer;
- endpoint declared by Bluetooth interface 0 (`e0/01/01`).

Both paths add `URB_ZERO_PACKET` before the USB core submits the transfer. The
Wi-Fi interface and unrelated USB devices are left unchanged.

The hook fails closed: if neither probe can be installed or kprobes are
disabled, the companion module refuses to load and the system `btusb` remains
unchanged.

## Before installing

First remove the earlier patched-`btusb` diagnostic build, if installed:

```bash
cd ../issue63-btusb-zlp
sudo ./btusb-zlp-test.sh remove
```

An obsolete `aic_btusb` installation must also be unloaded and removed before
this test. Verify that the AIC Bluetooth interfaces use the system driver:

```bash
lsusb -t
lsmod | grep -E '^(btusb|aic_btusb)\b'
```

## Install

Install the normal build dependencies. For Fedora:

```bash
sudo dnf install dkms gcc make kernel-devel-$(uname -r)
```

Then install and load the companion module:

```bash
cd tests/issue63-zlp-quirk
sudo ./aic-zlp-quirk-test.sh install
./aic-zlp-quirk-test.sh status
```

Expected status includes:

```text
quirk loaded:  yes
active hook:   btusb:alloc_bulk_urb
legacy aic_btusb: not loaded
AIC BT driver: btusb
```

`active hook: usb_submit_urb` is also valid when the distribution compiler
inlined the preferred private `btusb` function.

Select AAC and play audio for at least one hour:

```bash
bluetoothctl scan off
pactl set-card-profile bluez_card.28_6F_40_46_AB_B1 a2dp-sink-aac
```

While audio is playing, the `ZLP injections` counter should increase:

```bash
./aic-zlp-quirk-test.sh status
sudo dmesg | grep -i aic_zlp_quirk
```

## Remove

```bash
sudo ./aic-zlp-quirk-test.sh remove
./aic-zlp-quirk-test.sh status
```

Removal unloads only `aic_zlp_quirk.ko`. The distribution `btusb.ko` was never
overwritten and remains the Bluetooth transport throughout the test.

## Kernel requirements

- matching kernel headers;
- `CONFIG_KPROBES=y` and kretprobe support;
- standard `btusb` plus either its `alloc_bulk_urb` symbol or the USB core
  `usb_submit_urb` symbol;
- a valid module signature when Secure Boot policy requires one.

The `alloc_bulk_urb` function is present with the same signature in Linux
5.15, 6.1, 6.6, 6.12, 6.18, and 7.1.3, although compilers may inline it. The
fallback avoids depending on that implementation detail. DKMS compiles a
separate binary for each installed kernel while the repository maintains one
small source file.
