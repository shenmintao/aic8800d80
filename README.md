# AIC8800 Linux Wi-Fi and Bluetooth Driver
This driver supports AIC8800-family chipsets used by devices such as the Tenda U11, AX913B, and TP-Link Archer TX1U Nano.

> **Legacy MCU revision 1 branch:** You are viewing `legacy-mcu1`. This branch
> is only for AIC8800D80 or AIC8800DC/DW devices that report
> `chip_id=7, chip_mcu_id=1`. It provides complete matched V3 firmware and
> loader profiles for the D80 upload-limit failure validated in
> [issue #58](https://github.com/shenmintao/aic8800d80/issues/58) and the DC/DW
> V5 main-application timeout validated in
> [issue #71](https://github.com/shenmintao/aic8800d80/issues/71). Use
> [`main`](https://github.com/shenmintao/aic8800d80/tree/main) for newer
> `chip_mcu_id=0` hardware or when the MCU revision is unknown. See the
> [D80](tests/issue58-mcu1-legacy-fw/README.md) and
> [DC/DW](tests/issue71-mcu1-v3-profile/README.md) support notes before installing.
>
> After switching branches, rerun `sudo ./install.sh` and reboot; switching the
> Git branch alone does not replace firmware already installed under
> `/lib/firmware`.

Added support for devices with Vendor ID 368B (tested).

Tested on Linux kernel 6.16 with Ubuntu 25.04 and 6.1.0.27 with Debian 12.

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

> [!NOTE]
> **Maintained branches:** This repository now maintains only `main` and
> `legacy-mcu1`. You are viewing the branch for `chip_mcu_id=1`; do not switch
> this hardware to the former separate `bluetooth` branch, which is retired.
> Wi-Fi, the kernel's standard `btusb` Bluetooth path, and the device-scoped
> `368b:8d81` ZLP support are integrated here. The ZLP module remains inactive
> on other VID:PID combinations.

### Disclaimer
I did not develop this software, The code is sourced from the Tenda U11 driver. I only made some modifications to the code to adapt it to newer kernel versions. Apart from compilation issues, I am unable to address other problems.

### Attention
Before installing the driver, delete all aic8800-related folders under /lib/firmware. Using an incorrect firmware version may cause the system to freeze.

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

