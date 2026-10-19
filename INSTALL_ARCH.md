# Arch Linux / CachyOS install notes

Notes for installing the AIC8800D80 driver on Arch-based distributions.
The repository's [quick installer](INSTALL_SCRIPT.md) already supports
pacman and DKMS, including automatic rebuilds for kernel updates. This page
also covers the AUR package, manual installation, and device setup.

Choose the hardware branch described in the [README](README.md) first.
Adapters reporting `chip_mcu_id=1` need the matched `legacy-mcu1` driver and
firmware. The AUR package inspected for these notes tracks `main`; use the
repository installer or manual method from `legacy-mcu1` for MCU1 hardware.

## Tested device

- **Lukton DQ-211** — USB Wi-Fi 6 adapter, AICSEMI **AIC8800D80** chipset.
- Ships in CD mode as `1111:1111` (`Pandora International Ltd. 88M80`,
  `bcdDevice=10.01`, SCSI model `LGX WIFI6`, manufacturer `AIC`).
- The bundled virtual CD contains only `Wifi Driver install.exe` (a NSIS Windows
  installer); the vendor provides no Linux driver.
- After the Pandora `F3`+`F2` mode switch it re-enumerates as
  `a69c:8d80 aicsemi AIC Wlan`.

## Contributor-reported testing

- CachyOS, kernel `7.2.0-1-cachyos` and `6.18.42-1-cachyos-lts`.
- Wi-Fi 6 (HE) association and traffic confirmed; Bluetooth bound to `btusb`.

These results describe one Lukton DQ-211 unit, not all adapters sold as 88M80.

## Prerequisites

Update the system before installing build dependencies. Reboot first if the
update replaces the running kernel, so its matching headers are available:

```bash
sudo pacman -Syu
```

```bash
sudo pacman -S --needed base-devel dkms git usb_modeswitch usbutils iw
```

Install headers for **each kernel that should receive a DKMS build**. Examples:

| Kernel package | Header package |
| --- | --- |
| `linux` | `linux-headers` |
| `linux-lts` | `linux-lts-headers` |
| `linux-cachyos` | `linux-cachyos-headers` |

For example, on the standard Arch `linux` kernel:

```bash
sudo pacman -S --needed linux-headers
```

Choose the corresponding package for other CachyOS or custom kernels; installing
`linux-headers` alone does not provide headers for those kernels. See the
[Arch DKMS instructions](https://wiki.archlinux.org/title/Dynamic_Kernel_Module_Support).
Confirm the running kernel has a build tree before continuing:

```bash
uname -r
test -r "/usr/lib/modules/$(uname -r)/build/Makefile"
```

If the check fails, install the matching headers or reboot into the kernel
whose headers are installed. Use one installation method below to keep
package ownership and DKMS registration consistent.

## Option A: AUR package (recommended on Arch)

The `aic8800d80-dkms` package in the AUR packages this driver and its firmware
with DKMS, rebuilding automatically for installed kernels with matching headers.
With an already installed AUR helper such as `yay`, run as your normal user:

```bash
yay -S --needed aic8800d80-dkms
```

The package installs the firmware under `/lib/firmware/aic8800D80/` and installs
`aic.rules` to `/usr/lib/udev/rules.d/`.

### Check the mode-switch config file

`aic.rules` runs `usb_modeswitch -c /etc/usb_modeswitch.d/1111:1111`, but that
config file must exist or the switch fails. The inspected AUR package
(`pkgrel=6`, 2026-10-19) does not install it. If the file is missing, obtain
it from the repository (note the colon in the destination filename):

```bash
git clone https://github.com/shenmintao/aic8800d80.git
cd aic8800d80
sudo install -Dm644 usb_modeswitch/1111_1111 "/etc/usb_modeswitch.d/1111:1111"
sudo udevadm control --reload-rules
```

Unplug and reconnect the adapter after installing the file. Without it, this
rule cannot switch the reported unit out of CD mode (`1111:1111`).

### Refresh the module index

DKMS normally refreshes the module index during installation. If
`modprobe aic8800_fdrv` reports `Module not found`, first check that DKMS
reports the module as **installed for the running kernel**:

```bash
dkms status
uname -r
```

If that kernel has no installed module, resolve its headers/build failure;
`depmod` does not build missing modules. If the module is already installed,
refresh the index and retry:

```bash
sudo depmod -a "$(uname -r)"
sudo modprobe aic8800_fdrv
```

## Option B: manual install from this repository

After the prerequisites above, build first. For MCU1 hardware, replace
`--branch main` with `--branch legacy-mcu1` in the clone command:

```bash
git clone --branch main https://github.com/shenmintao/aic8800d80.git
cd aic8800d80
make -C drivers/aic8800
```

Only after the build succeeds, install from the same repository directory:

```bash
# remove any previous AIC firmware first — a mismatched version can freeze the system
sudo rm -rf /lib/firmware/aic8800*

sudo cp -r ./fw/aic8800* /lib/firmware/
sudo install -Dm644 aic.rules /usr/lib/udev/rules.d/aic.rules
sudo install -Dm644 usb_modeswitch/1111_1111 "/etc/usb_modeswitch.d/1111:1111"
sudo make -C drivers/aic8800 install
sudo udevadm control --reload-rules
sudo modprobe aic8800_fdrv
```

Unplug and reconnect the adapter. The manual path has no DKMS: after kernel
updates, install matching headers and rerun these commands from the repository:

```bash
make -C drivers/aic8800 clean
make -C drivers/aic8800
sudo make -C drivers/aic8800 install
```

The install target runs `depmod` itself. Prefer a DKMS method for automatic
rebuilds on rolling-release distributions.

## Why the driver loads automatically after the switch

`aic_load_fw` carries the USB alias `usb:vA69Cp8D80d*`, so once the adapter
re-enumerates as `a69c:8d80` the kernel loads the driver on its own — no
`modules-load.d` entry is needed. The full plug sequence is:

`1111:1111` → `aic.rules` → `usb_modeswitch -c /etc/usb_modeswitch.d/1111:1111`
→ `a69c:8d80` → kernel autoloads `aic_load_fw` → firmware upload
→ operational USB identity → `aic8800_fdrv` → wireless interface.

`a69c:8d80` is the firmware-loader identity; reaching it alone does not mean
Wi-Fi initialization succeeded.

## Connecting and route priority (NetworkManager)

The following commands assume NetworkManager is installed and managing Wi-Fi.
Find the adapter's interface name first (`wlan1` is only an example):

```bash
nmcli device status
nmcli device wifi connect "<SSID>" ifname wlan1 password "<PSK>"
```

To prefer the adapter over the onboard radio, give it a lower route metric:

```bash
sudo nmcli con mod "<SSID> 1" ipv4.route-metric 300 ipv6.route-metric 300
sudo nmcli con up "<SSID> 1"
```

NetworkManager can add a `20000` route-metric penalty when an interface fails
its global connectivity check, which can explain an observed `300` → `20300`.
Check connectivity before treating this as a driver or metric-setting failure;
see the [NetworkManager connectivity documentation](https://networkmanager.pages.freedesktop.org/NetworkManager/NetworkManager/NetworkManager.conf.html).

```bash
nmcli networking connectivity check
```

If you intentionally want only the adapter's connection active, disable
autoconnect on the onboard profile and disconnect it:

```bash
sudo nmcli con mod "<onboard-profile>" connection.autoconnect no
sudo nmcli con down "<onboard-profile>"
```

## Verification

```bash
lsusb | grep -iE "1111:1111|a69c|368b"
lsmod | grep -E "aic8800_fdrv|aic_load_fw"
iw dev
nmcli device status
ip route show default
sudo dmesg | grep -i aic
```

The log line `HE supp 1` indicates HE capability. Use `iw dev <interface> link`
and actual network traffic to check the active connection.

## Notes

- The onboard-vs-adapter interface names are not fixed; confirm with `iw dev`.
- The reported unit used the vendor `F3`+`F2` sequence. Other adapters sold
  under similar names can require different switching or firmware profiles;
  see [issue #79](https://github.com/shenmintao/aic8800d80/issues/79).
