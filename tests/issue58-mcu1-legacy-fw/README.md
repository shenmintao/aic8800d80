# AIC8800D80 legacy MCU revision 1 support

The `legacy-mcu1` branch is the maintained compatibility profile for the older
AIC8800D80 MCU revision tracked in
[issue #58](https://github.com/shenmintao/aic8800d80/issues/58), with related
reports in issues #14, #64, and #65. It is intended only for devices that log:

```text
chip_id=7, chip_mcu_id=1
```

Affected adapters have been observed entering through `a69c:572f` or a similar
mass-storage ID, changing to `a69c:8d80` for firmware loading, and failing when
the SDK V5 FMAC upload reaches address `0x170400`.

## Root cause

The affected MCU revision cannot accept the complete SDK V5 FMAC image in its
available upload window. With an upload base of `0x120000`, the deterministic
failure at `0x170400` gives a limit of about `0x50400` (328,704) bytes.

| Firmware | Size | End address (exclusive) | Relative to `0x170400` |
| --- | ---: | ---: | ---: |
| SDK V5 FMAC on `main` | 358,072 bytes | `0x1776B8` | 29,368 bytes over |
| SDK V3 FMAC on this branch | 327,037 bytes | `0x16FD7D` | 1,667 bytes free |

Replacing only the FMAC image is not sufficient. Earlier mixed-firmware tests
could enumerate but failed to provide a usable radio. This branch therefore
uses the complete matching AIC8800D80 firmware set from Radxa SDK V3, fixed at
upstream commit
[`254d47e6a131dbed5ba32131972f4719f3e1c7fe`](https://github.com/radxa-pkg/aic8800/commit/254d47e6a131dbed5ba32131972f4719f3e1c7fe),
together with its matching FMAC patch table and fixed patch-buffer layout.

For `chip_mcu_id=1`, the loader also reads register `0x40100020`, sets bit 0,
and writes it back before firmware upload. This is the MCU1 Bluetooth cache fix
identified in [PR #35](https://github.com/shenmintao/aic8800d80/pull/35).
Bluetooth continues to use the kernel's standard `btusb` driver; this branch
does not contain or install `aic_btusb`.

Firmware and loader paths for D80N, D80X2, DC, and other variants are unchanged.

## Hardware validation

The final profile was validated on a Steren COM-8231+ reporting
`chip_id=7, chip_mcu_id=1`. Test commit
[`027a7a8`](https://github.com/shenmintao/aic8800d80/commit/027a7a8)
demonstrated all of the following:

- the 327,037-byte FMAC uploads completely without `cmd timed-out` or
  `bin upload fail: 170400`;
- the adapter completes `a69c:572f -> a69c:8d80 -> a69c:8d81` enumeration;
- 2.4 GHz and 5 GHz Wi-Fi scan, association, DHCP, and traffic work;
- USB interfaces 0 and 1 bind to system `btusb`, while interface 2 binds to
  `aic8800_fdrv`;
- the earlier HCI Reset timeout `Opcode 0x0c03 failed: -110` is gone;
- Bluetooth scan and a real connection both succeed.

## Install

From an existing clone:

```bash
git fetch origin
git switch legacy-mcu1
git pull --ff-only
sudo ./install.sh
sudo reboot
```

The installer replaces the installed AIC firmware. Switching Git branches
without rerunning `install.sh` does not change the active firmware.

The expected V3 FMAC can be confirmed with:

```bash
stat -c '%s bytes' /lib/firmware/aic8800D80/fmacfw_8800d80_u02.bin
sha256sum /lib/firmware/aic8800D80/fmacfw_8800d80_u02.bin
```

Expected output:

```text
327037 bytes
1ec680c2b63dcaa0e5d33c5fb6d1857d030f8145c05c385e243760388a61a0da
```

## Verify

After reboot or a physical disconnect/reconnect, check the complete device:

```bash
sudo dmesg | grep -iE 'AIC8800D80|chip_id|chip_mcu_id|fmacfw|bin upload|cmd timed-out|Bluetooth|0x0c03|error -110'
lsusb -t
iw dev
bluetoothctl list
```

The initialization log should contain:

```text
AIC8800D80 legacy: using Radxa SDK V3 loader profile
AIC8800D80 MCU1: enabled Bluetooth cache fix
```

`lsusb -t` should show Bluetooth interfaces using `btusb` and the Wi-Fi
interface using `aic8800_fdrv`. Verify traffic through the AIC network interface
rather than an onboard Wi-Fi adapter or phone/RNDIS connection.

## Return to current firmware

Newer `chip_mcu_id=0` hardware should use `main`. Reinstall after switching so
that the V5 firmware is restored under `/lib/firmware`:

```bash
git switch main
git pull --ff-only
sudo ./install.sh
sudo reboot
```
