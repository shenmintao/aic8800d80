# Issue #63: system `btusb` ZLP test

This test keeps the normal `aic_load_fw + btusb` architecture. It does not
restore or install the vendor `aic_btusb` driver.

The patch adds one quirk to the standard Linux `btusb` source:

- match only AICSemi `368b:8d81`;
- set `URB_ZERO_PACKET` on Bluetooth ACL bulk OUT transfers;
- leave every other `btusb` device and code path unchanged.

The test installer downloads the standard `btusb` source matching the running
kernel's upstream version, applies the patch, and installs the resulting
`btusb.ko` through DKMS. The distribution module is not overwritten and can be
restored by removing the DKMS test package.

## Install the test module

On Fedora, make sure the test build dependencies are installed:

```bash
sudo dnf install dkms gcc make kernel-devel-$(uname -r) curl patch
```

Stop Bluetooth scanning and disconnect Bluetooth devices, then run:

```bash
cd tests/issue63-btusb-zlp
sudo ./btusb-zlp-test.sh install
./btusb-zlp-test.sh status
```

`modinfo -n btusb` should point to an `updates/dkms` path. Secure Boot systems
may require the DKMS module to be signed or enrolled before it can load.

Select AAC again and play audio for at least one hour:

```bash
bluetoothctl scan off
pactl set-card-profile bluez_card.28_6F_40_46_AB_B1 a2dp-sink-aac
```

Please capture `sudo btmon -w issue63-zlp.btsnoop` and relevant `dmesg` lines if
the connection still stalls.

## Restore the distribution module

```bash
sudo ./btusb-zlp-test.sh remove
./btusb-zlp-test.sh status
```

After removal, `modinfo -n btusb` should point back to the distribution's
`kernel/drivers/bluetooth/btusb.ko` path.

## Scope

This is an A/B diagnostic patch, not a production workaround. If AAC remains
stable, the small kernel patch can be submitted upstream. If it still fails,
the next isolated experiment should add an AIC-specific runtime-PM quirk rather
than replacing the standard Bluetooth driver.
