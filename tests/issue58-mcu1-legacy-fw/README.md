# Issue #58: legacy firmware test for D80 MCU revision 1

This branch is an experimental build for
[issue #58](https://github.com/shenmintao/aic8800d80/issues/58). It is intended
only for AIC8800D80 devices reported as:

```text
chip_id=7, chip_mcu_id=1
```

Do not merge this branch as a general firmware downgrade. It replaces the
complete `fw/aic8800D80` firmware set so that an affected device can test one
specific hypothesis. Firmware for D80N, D80X2, DC, and other variants is not
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

## Install the test branch

From an existing clone:

```bash
git fetch origin
git switch test/issue-58-mcu1-legacy-fw
sudo ./install.sh
```

Disconnect and reconnect the USB device after installation. Reboot if the
device does not re-enumerate cleanly.

## Verify

First save the complete kernel log and confirm that firmware upload passes the
old failure address without a command timeout:

```bash
sudo dmesg -C
# Disconnect and reconnect the device, then wait for initialization.
sudo dmesg | tee issue58-legacy-fw-dmesg.txt
sudo dmesg | grep -iE 'aic|chip_id|chip_mcu_id|fmacfw|bin upload|cmd timed-out'
```

Please report all of the following, even if an earlier item fails:

1. The `chip_id` and `chip_mcu_id` lines, and whether firmware upload completes.
2. Whether nearby SSIDs can be scanned.
3. Whether Wi-Fi association succeeds.
4. Whether the interface receives an address by DHCP.
5. Whether the gateway and an Internet address can be pinged, and whether real
   traffic works.
6. Whether Bluetooth still enumerates and works through the kernel's standard
   `btusb` driver. This test does not install or use `aic_btusb`.

Useful commands:

```bash
iw dev
nmcli device wifi list
ip address
ip route
ping -c 4 "$(ip route | awk '/default/ {print $3; exit}')"
ping -c 4 1.1.1.1
lsusb -t
bluetoothctl list
```

If NetworkManager is unavailable, identify the interface with `iw dev`, then
replace `wlan0` below with that interface name:

```bash
sudo iw dev wlan0 scan | grep SSID
```

## Return to the current V5 firmware

The installer replaces the firmware under `/lib/firmware`, so merely switching
Git branches is not enough. Reinstall after returning to `main`:

```bash
git switch main
git pull --ff-only
sudo ./install.sh
sudo reboot
```
