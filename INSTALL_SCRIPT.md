# Automated Installation Script

## Overview

This automated installation script (`install.sh`) simplifies the process of installing the AIC8800D80 WiFi driver on Linux systems.

- Automatic Secure Boot detection
- Automatic dependency installation
- Automatic driver compilation and installation
- Automatic module loading on boot
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