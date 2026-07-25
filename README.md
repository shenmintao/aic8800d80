# AIC8800D80 Linux Driver
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
> Hardware revision takes precedence over the Bluetooth feature branch. MCU1
> devices should remain on `legacy-mcu1`, which initializes Bluetooth for the
> kernel's standard `btusb` driver. After switching branches, rerun
> `sudo ./install.sh` and reboot; switching the Git branch alone does not
> replace the firmware already installed under `/lib/firmware`.

Added support for devices with Vendor ID 368B (tested).

Tested on Linux kernel 6.16 with Ubuntu 25.04 and 6.1.0.27 with Debian 12.

> **Bluetooth Support**: The [`bluetooth`](https://github.com/shenmintao/aic8800d80/tree/bluetooth) branch fully supports Bluetooth. This main branch only provides Wi-Fi functionality. Please switch to the `bluetooth` branch if you need Bluetooth support.

### Disclaimer
I did not develop this software, The code is sourced from the Tenda U11 driver. I only made some modifications to the code to adapt it to newer kernel versions. Apart from compilation issues, I am unable to address other problems.

### Attention
Before installing the driver, delete all aic8800-related folders under /lib/firmware. Using an incorrect firmware version may cause the system to freeze.

### Installation Steps

#### Method 1: [Quick Installation](INSTALL_SCRIPT.md) (Recommended)

#### Method 2: Manual Installation

#### Copy udev rules:
Copy the aic.rules file to /lib/udev/rules.d/:

```bash
sudo cp aic.rules /lib/udev/rules.d/
```

#### Copy firmware:

Copy the aic8800D80 folder from ./fw to /lib/firmware/:

```bash
sudo cp -r ./fw/aic8800D80 /lib/firmware/
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

