# Automated Installation Script

## Overview

This automated installation script (`install.sh`) installs the AIC8800 Wi-Fi
driver and firmware loader on Linux systems. Wi-Fi/Bluetooth combo adapters use
the standard kernel `btusb` driver after firmware initialization. For USB
device `368b:8d81`, it also installs a device-scoped ACL bulk TX ZLP companion
module without replacing the distribution's `btusb.ko`.

- Automatic Secure Boot detection
- Automatic dependency installation
- Automatic driver compilation and installation
- Automatic module loading on boot
- Automatic cleanup of obsolete `aic_btusb` configuration
- Automatic `aic_zlp_quirk` handling for the validated `368b:8d81` device
- Wi-Fi-only and Wi-Fi/Bluetooth combo adapter support from the same branch
- Comprehensive error handling
- Colored output and logging
- Compatible with Ubuntu, Debian, Fedora, and derivatives (DKMS supported `dkms.conf`)

## Usage

```bash
# Clone the repository and download the script, make the script executable
git clone https://github.com/shenmintao/aic8800d80.git && cd aic8800d80 && chmod +x install.sh

# Run the installation
sudo ./install.sh
```

> For more information, please refer to the [README.md](README.md) file.

<br>

---

<br>

# Diagnostic Build Script (Optional)

This script is used to diagnose build issues with the AIC8800D80 driver. Useful for identifying the root cause of the build failure.

```bash
# Make the script executable and run it
chmod +x diagnostic_build.sh && sudo ./diagnostic_build.sh
```
