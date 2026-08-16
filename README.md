# AIC8800 Linux Wi-Fi and Bluetooth Driver
This driver is for the AIC8800D80 chipset, supported by devices such as the Tenda U11 and AX913B.

> [!IMPORTANT]
> **Choose the branch for your hardware revision before installing.** If the
> driver log reports `chip_id=7, chip_mcu_id=1` on an AIC8800D80 or
> AIC8800DC/DW device, use the
> [`legacy-mcu1`](https://github.com/shenmintao/aic8800d80/tree/legacy-mcu1)
> branch. It contains the matched legacy firmware and loader profile required
> to avoid the deterministic firmware upload timeout at `0x170400`. Use
> `main` when `chip_mcu_id=0` or when the MCU revision is unknown.
>
> Hardware revision determines the firmware branch. MCU1 devices should use
> `legacy-mcu1`, which also initializes Bluetooth for the
> kernel's standard `btusb` driver. After switching branches, rerun
> `sudo ./install.sh` and reboot; switching the Git branch alone does not
> replace the firmware already installed under `/lib/firmware`.

Added support for devices with Vendor ID 368B (tested).

Tested on Linux kernel 6.16 with Ubuntu 25.04 and 6.1.0.27 with Debian 12.

> [!NOTE]
> **Maintained branches:** This repository now maintains two hardware branches:
> `main` for `chip_mcu_id=0` or an unknown MCU revision, and `legacy-mcu1` for
> `chip_mcu_id=1`. Wi-Fi, the kernel's standard `btusb` Bluetooth path, and the
> device-scoped ZLP support are integrated into both branches. The former
> separate `bluetooth` branch is retired and should no longer be used.

The same driver supports Wi-Fi-only adapters and Wi-Fi/Bluetooth combo
adapters. On combo devices, `aic_load_fw` uploads the AIC firmware and the
standard Linux `btusb` driver handles the Bluetooth HCI interface. The obsolete
custom `aic_btusb` module is not used.

USB device `368b:8d81` also uses the bundled `aic_zlp_quirk` companion module.
It adds the Bluetooth ACL bulk TX zero-length-packet behavior validated in
[issue #63](https://github.com/shenmintao/aic8800d80/issues/63), while leaving
the distribution's original `btusb.ko` installed and bound to the device. The
quirk is filtered to that VID:PID and fails closed when the required kernel
probe support is unavailable.

### Disclaimer
I did not develop this software, The code is sourced from the Tenda U11 driver. I only made some modifications to the code to adapt it to newer kernel versions. Apart from compilation issues, I am unable to address other problems.

### Attention
Before installing the driver, delete all aic8800-related folders under /lib/firmware. Using an incorrect firmware version may cause the system to freeze.

#### Pandora 88M80 mode switching

Pandora 88M80 adapters that initially appear as USB device `1111:1111` are
automatically switched to `a69c:8d80` by sending the required `F3` then `F2`
commands. Existing users must rerun `sudo ./install.sh` to replace the installed
usb_modeswitch configuration, then unplug the adapter completely and plug it
back in.

If automatic switching does not occur, try the same sequence manually:

```bash
sudo usb_modeswitch -v 1111 -p 1111 \
  -M "555342438765432100000000000010fd0000000000000000000000000000f3" \
  -2 "555342438765432100000000000010fd0000000000000000000000000000f2"
```

This only addresses switching from `1111:1111` to `a69c:8d80`. If the device
has reached `a69c:8d80` but firmware startup still fails, see
[issue #79](https://github.com/shenmintao/aic8800d80/issues/79).

### Installation Steps

#### Method 1: [Quick Installation](INSTALL_SCRIPT.md) (Recommended)

#### Method 2: Manual Installation

#### Copy udev rules:
Copy the aic.rules file to /usr/lib/udev/rules.d/:

```bash
sudo cp aic.rules /usr/lib/udev/rules.d/
```

#### Copy firmware:

Copy the firmware directories from `./fw` to `/lib/firmware/`:

```bash
sudo cp -r ./fw/aic8800* /lib/firmware/
```
#### Navigate to the driver directory:

Change to the drivers/aic8800 directory:

```bash
cd ./drivers/aic8800
```

#### Compile and Install the Driver:

First, compile the driver:

```bash
make
```
Then, install the driver:

```bash
sudo make install
```

For any kernel updates, you'll need to reinstall the driver:

```bash
make clean
make
sudo make install
```

### Load the Driver
After installation, load the driver with the following command:

```bash
sudo modprobe aic8800_fdrv
```

### Verify the Module is Active
Check if the module is loaded correctly:

```bash
lsmod | grep aic
```
You should see output similar to:

```bash
aic8800_fdrv    536576  0
cfg80211        1146880 1   aic8800_fdrv
aic_load_fw     69632   1   aic8800_fdrv
usbcore         348160  10  xhci_hcd,ehci_pci,usbhid,usb_storage,ehci_hcd,xhci_pci,uas,aic_load_fw,uhci_hcd,aic8800_fdrv
```

After that, plug in your USB wireless network card.

### Verify Wi-Fi Device is Active
To check if the Wi-Fi interface is recognized, run:

```bash
iwconfig
```
If the device is still not active, check the kernel logs for any errors related to the driver:

```bash
sudo dmesg
```

### Bluetooth on Combo Adapters

Bluetooth support does not require a separate AIC transport module. After
`aic_load_fw` initializes a combo adapter, the kernel automatically binds its
Bluetooth interface to `btusb`. A Wi-Fi-only adapter does not expose that
interface, so the Bluetooth path remains inactive.

Verify the expected modules and controller with:

```bash
lsmod | grep -E 'aic_load_fw|aic8800_fdrv|aic_zlp_quirk|btusb'
lsusb -t
bluetoothctl list
```

To scan after a controller appears:

```bash
bluetoothctl
power on
scan on
```

If Bluetooth is missing or reports HCI timeouts, run the read-only diagnostic
script and attach its output together with the current boot log:

```bash
chmod +x diagnose_bt.sh
sudo ./diagnose_bt.sh
sudo journalctl -k -b --no-pager
```

The installer removes active references to the retired `aic_btusb` integration.
It does not force-load `btusb` or globally change the Bluetooth rfkill state;
normal kernel device matching and the user's system policy remain in control.

For `368b:8d81`, verify the ZLP hook and its injection counter while Bluetooth
traffic is active:

```bash
cat /sys/module/aic_zlp_quirk/parameters/hook
cat /sys/module/aic_zlp_quirk/parameters/injections
```

The Wi-Fi-reset recovery behavior tracked in
[issue #53](https://github.com/shenmintao/aic8800d80/issues/53) remains a known
limitation: after an airplane-mode or hotspot reset, Bluetooth may require a
physical unplug/replug of the adapter.

