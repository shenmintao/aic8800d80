# Issue #58: legacy firmware test for D80 MCU revision 1

This branch is an experimental build for
[issue #58](https://github.com/shenmintao/aic8800d80/issues/58). It is intended
only for AIC8800D80 devices reported as:

```text
chip_id=7, chip_mcu_id=1
```

Do not merge this branch as a general firmware downgrade. It replaces the
complete `fw/aic8800D80` firmware set and matches the D80 loader's FMAC patch
table, patch-buffer layout, and MCU cache setup to that firmware generation.
The USB transport and modern-kernel compatibility code remain current.
Firmware and loader paths for D80N, D80X2, DC, and other variants are not
changed.

## Hypothesis

The affected device times out when an FMAC upload reaches address `0x170400`.
With an upload base of `0x120000`, this suggests that the old MCU revision has
an FMAC RAM window of about `0x50400` (328,704) bytes.

| Firmware | Size | End address (exclusive) | Result relative to `0x170400` |
| --- | ---: | ---: | ---: |
| Current V5 FMAC | 358,072 bytes | `0x1776B8` | 29,368 bytes over |
| Legacy V3 FMAC in this branch | 327,037 bytes | `0x16FD7D` | 1,667 bytes free |

This branch uses the complete matching D80 firmware set from Radxa SDK V3,
fixed at upstream commit
[`254d47e6a131dbed5ba32131972f4719f3e1c7fe`](https://github.com/radxa-pkg/aic8800/commit/254d47e6a131dbed5ba32131972f4719f3e1c7fe).
The normal FMAC file has SHA-256
`1ec680c2b63dcaa0e5d33c5fb6d1857d030f8145c05c385e243760388a61a0da`.

The first hardware test proved that this image uploads completely without the
`0x170400` timeout, but the device then failed to re-enumerate while the newer
loader was still applying SDK V5-era FMAC patches and MCU cache setup. This
follow-up matches those loader operations to V3 so that their effect can be
tested separately, while retaining unrelated fixes in the current driver.

## Install the test branch

From an existing clone:

```bash
git fetch origin
git switch test/issue-58-mcu1-legacy-fw
git pull --ff-only
sudo ./install.sh
```

Disconnect and reconnect the USB device after installation. Reboot if the
device does not re-enumerate cleanly.

## Verify

First save the complete kernel log and confirm that the V3 loader profile is
active, firmware upload passes the old failure address, and the adapter
re-enumerates after `a69c:8d80`:

```bash
sudo dmesg -C
# Disconnect and reconnect the device, then wait for initialization.
sudo dmesg | tee issue58-legacy-fw-dmesg.txt
sudo dmesg | grep -iE 'aic|issue58|chip_id|chip_mcu_id|fmacfw|bin upload|cmd timed-out|error -110'
lsusb
lsusb -t
```

The log must contain:

```text
issue58: using Radxa SDK V3 D80 loader profile
```

Please report all of the following, even if an earlier item fails:

1. The `chip_id` and `chip_mcu_id` lines, and whether firmware upload completes.
2. Whether the adapter re-enumerates and creates an interface owned by
   `aic8800_fdrv`.
3. Whether nearby SSIDs can be scanned through that AIC interface.
4. Whether Wi-Fi association succeeds through that interface.
5. Whether the interface receives an address by DHCP.
6. Whether the gateway and an Internet address can be pinged, and whether real
   traffic works.
7. Whether Bluetooth still enumerates and works through the kernel's standard
   `btusb` driver. This test does not install or use `aic_btusb`.

Useful commands:

```bash
iw dev
nmcli device status
nmcli device wifi list
ip address
ip route
lsusb -t
bluetoothctl list
```

Identify the new AIC interface with `iw dev`, then replace `wlan0` below with
that interface name. Confirm its driver before treating scan or traffic from
another onboard adapter as a successful result:

```bash
readlink -f /sys/class/net/wlan0/device/driver
sudo iw dev wlan0 scan | grep SSID
ip route show dev wlan0
ping -I wlan0 -c 4 1.1.1.1
```

The driver path should end in `/aic8800_fdrv`.

## Return to the current V5 firmware

The installer replaces the firmware under `/lib/firmware`, so merely switching
Git branches is not enough. Reinstall after returning to `main`:

```bash
git switch main
git pull --ff-only
sudo ./install.sh
sudo reboot
```
