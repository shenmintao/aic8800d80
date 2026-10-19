# Arch Linux / CachyOS install notes

Notes for installing the AIC8800D80 driver on Arch-based distributions, where
the `install.sh` helper (written for Ubuntu/Debian/Fedora and derivatives) is
not the most convenient path or cannot manage DKMS rebuilds for rolling
kernels.

## Tested device

- **Lukton DQ-211** — USB Wi-Fi 6 adapter, AICSEMI **AIC8800D80** chipset.
- Ships in CD mode as `1111:1111` (`Pandora International Ltd. 88M80`,
  `bcdDevice=10.01`, SCSI model `LGX WIFI6`, manufacturer `AIC`).
- The bundled virtual CD contains only `Wifi Driver install.exe` (a NSIS Windows
  installer); the vendor provides no Linux driver.
- After the Pandora `F3`+`F2` mode switch it re-enumerates as
  `a69c:8d80 aicsemi AIC Wlan`.

## Tested on

- CachyOS, kernel `7.2.0-1-cachyos` and `6.18.42-1-cachyos-lts`.
- Wi-Fi 6 (HE) association and traffic confirmed; Bluetooth bound to `btusb`.

## Option A: AUR package (recommended on Arch)

The `aic8800d80-dkms` package in the AUR packages this driver and its firmware
with DKMS, rebuilding automatically for each installed kernel:

```bash
sudo pacman -S --needed dkms base-devel
yay -S --needed aic8800d80-dkms
```

The package installs the firmware under `/lib/firmware/aic8800D80/` and installs
`aic.rules` to `/usr/lib/udev/rules.d/`.

### The mode-switch config file is not shipped by the AUR package

`aic.rules` runs `usb_modeswitch -c /etc/usb_modeswitch.d/1111:1111`, but that
config file must exist or the switch is a no-op. Copy it from the repository
(note the colon in the filename on Linux):

```bash
sudo cp usb_modeswitch/1111_1111 "/etc/usb_modeswitch.d/1111:1111"
sudo udevadm control --reload
```

Without this file the adapter stays in CD mode (`1111:1111`) and no interface
appears.

### Refresh the module index

The DKMS modules land in `updates/dkms/`, which the kernel only finds after
`depmod`. If `modprobe aic8800_fdrv` reports `Module not found` right after
installation:

```bash
sudo depmod -a "$(uname -r)"
sudo modprobe aic8800_fdrv
```

## Option B: manual install from this repository

```bash
git clone https://github.com/shenmintao/aic8800d80.git
cd aic8800d80

# remove any previous AIC firmware first — a mismatched version can freeze the system
sudo rm -rf /lib/firmware/aic8800*

sudo cp aic.rules /usr/lib/udev/rules.d/
sudo cp -r ./fw/aic8800* /lib/firmware/
sudo cp usb_modeswitch/1111_1111 "/etc/usb_modeswitch.d/1111:1111"
sudo udevadm control --reload

cd drivers/aic8800
make
sudo make install
sudo depmod -a "$(uname -r)"
sudo modprobe aic8800_fdrv
```

The manual path has no DKMS: after every kernel update you must rerun
`make clean && make && sudo make install`. Prefer Option A on rolling-release
distributions.

## Why the driver loads automatically after the switch

`aic_load_fw` carries the USB alias `usb:vA69Cp8D80d*`, so once the adapter
re-enumerates as `a69c:8d80` the kernel loads the driver on its own — no
`modules-load.d` entry is needed. The full plug sequence is:

`1111:1111` → `aic.rules` → `usb_modeswitch -c /etc/usb_modeswitch.d/1111:1111`
→ `a69c:8d80` → kernel autoloads `aic_load_fw` → firmware upload → `wlanX`.

## Connecting and route priority (NetworkManager)

The adapter shows up as a second wireless interface (for example `wlan1`, next
to an onboard `wlan0`):

```bash
nmcli device status
nmcli device wifi connect "<SSID>" ifname wlan1 password "<PSK>"
```

To prefer the adapter over the onboard radio, give it a lower route metric:

```bash
sudo nmcli con mod "<SSID> 1" ipv4.route-metric 300 ipv6.route-metric 300
sudo nmcli con up "<SSID> 1"
```

Note: on some NetworkManager versions the effective DHCP route metric ends up
offset (observed `300` → `20300`), which still leaves the onboard radio
preferred. If that happens, disable autoconnect on the onboard profile so the
adapter owns the default route:

```bash
sudo nmcli con mod "<onboard-profile>" connection.autoconnect no
sudo nmcli con down "<onboard-profile>"
```

## Verification

```bash
lsusb | grep -E "1111:1111|a69c"
lsmod | grep -E "aic8800_fdrv|aic_load_fw"
iw dev
nmcli device status
ip route show default
sudo dmesg | grep -i aic
```

A successful bring-up shows `HE supp 1` in the driver log (802.11ax / Wi-Fi 6)
and a new `wlanX` interface.

## Notes

- The onboard-vs-adapter interface names are not fixed; confirm with `iw dev`.
- As with other Pandora clones, `usb_modeswitch -K/-J` and `eject` do not
  perform the switch — only the vendor `F3`+`F2` CDBs do.
