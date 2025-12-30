#!/bin/bash

# Cores for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Variables
DRIVER_REPO="https://github.com/shenmintao/aic8800d80.git"
DRIVER_DIR="$HOME/aic8800d80"
LOG_FILE="/tmp/aic8800d80_install.log"
MODULE_NAME="aic8800_fdrv"

# Auxiliary functions
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
    echo "[INFO] $1" >> "$LOG_FILE"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
    echo "[SUCCESS] $1" >> "$LOG_FILE"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
    echo "[WARNING] $1" >> "$LOG_FILE"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
    echo "[ERROR] $1" >> "$LOG_FILE"
}

print_step() {
    echo -e "\n${CYAN}==>${NC} ${1}"
    echo "==> $1" >> "$LOG_FILE"
}

# Function to check if the command was successful
check_success() {
    if [ $? -eq 0 ]; then
        print_success "$1"
        return 0
    else
        print_error "$2"
        return 1
    fi
}

#############################################################################
# Check Secure Boot
#############################################################################

check_secure_boot() {
    print_step "Checking Secure Boot status..."
    
    # Method 1: Check via mokutil (more reliable)
    if command -v mokutil &> /dev/null; then
        SECURE_BOOT_STATUS=$(mokutil --sb-state 2>/dev/null | grep -i "SecureBoot" | awk '{print $2}')
        
        if [[ "$SECURE_BOOT_STATUS" == "enabled" ]]; then
            print_error "Secure Boot is ENABLED!"
            echo ""
            echo -e "${RED}╔════════════════════════════════════════════════════════════════════╗${NC}"
            echo -e "${RED}║  SECURE BOOT IS ENABLED - INSTALLATION CANNOT PROCEED              ║${NC}"
            echo -e "${RED}╚════════════════════════════════════════════════════════════════════╝${NC}"
            echo ""
            echo -e "${YELLOW}Why does Secure Boot need to be disabled?${NC}"
            echo ""
            echo "Secure Boot is a security feature of UEFI that prevents the loading"
            echo "of unsigned kernel modules. The AIC8800D80 driver is a custom module"
            echo "compiled locally and does not have a digital signature recognized by the system."
            echo ""
            echo -e "${CYAN}How to disable Secure Boot:${NC}"
            echo ""
            echo "1. Restart the computer"
            echo "2. Enter the BIOS/UEFI (usually pressing DEL, F2, F10 or ESC)"
            echo "3. Search for 'Secure Boot' in the settings (usually in Security)"
            echo "4. Change from 'Enabled' to 'Disabled'"
            echo "5. Save the settings and restart (usually F10)"
            echo "6. Execute this script again"
            echo ""
            echo -e "${YELLOW}Note:${NC} Disabling Secure Boot is safe for personal use, but"
            echo "      slightly reduces security against bootrootkits."
            echo ""
            return 1
        else
            print_success "Secure Boot is disabled. Proceeding..."
            return 0
        fi
    fi
    
    # Method 2: Check via /sys/firmware/efi/efivars (fallback)
    if [ -d /sys/firmware/efi/efivars ]; then
        if [ -f /sys/firmware/efi/efivars/SecureBoot-* ]; then
            SB_VALUE=$(od -An -t u1 /sys/firmware/efi/efivars/SecureBoot-* | awk '{print $NF}')
            if [ "$SB_VALUE" = "1" ]; then
                print_error "Secure Boot is ENABLED (detected via EFI vars)!"
                echo ""
                echo -e "${RED}Secure Boot needs to be disabled to install this driver.${NC}"
                echo "See the instructions above on how to disable it."
                return 1
            else
                print_success "Secure Boot is disabled (verified via EFI vars)."
                return 0
            fi
        fi
    fi
    
    # Method 3: Check via dmesg (last resort)
    DMESG_SB=$(dmesg | grep -i "secure boot" | grep -i "enabled")
    if [ -n "$DMESG_SB" ]; then
        print_warning "Secure Boot may be enabled (detected via dmesg)."
        echo "Please check manually and disable if necessary."
        read -p "Do you want to continue anyway? (y/N): " choice
        if [[ ! "$choice" =~ ^[Ss]$ ]]; then
            return 1
        fi
    fi
    
    print_success "Secure Boot verification completed."
    return 0
}

#############################################################################
# Check requirements
#############################################################################

check_requirements() {
    print_step "Checking system requirements..."
    
    # Verificar se é root
    if [ "$EUID" -eq 0 ]; then
        print_error "This script MUST NOT be run as root!"
        echo "Run as a normal user. The script will request sudo when necessary."
        exit 1
    fi
    
    # Check kernel
    KERNEL_VERSION=$(uname -r)
    print_info "Kernel detected: $KERNEL_VERSION"
    
    # Check if has access to sudo
    if ! sudo -n true 2>/dev/null; then
        print_info "This script requires sudo privileges."
        sudo -v || { print_error "Failed to obtain sudo privileges."; exit 1; }
    fi
    
    print_success "Basic requirements met."
}

#############################################################################
# Install dependencies
#############################################################################

install_dependencies() {
    print_step "Installing necessary dependencies..."
    
    # Update package list
    print_info "Updating package list..."
    sudo apt update >> "$LOG_FILE" 2>&1
    check_success "Package list updated" "Failed to update package list"
    
    # List of dependencies
    DEPENDENCIES=(
        "git"
        "build-essential"
        "dkms"
        "bc"
        "linux-headers-$(uname -r)"
        "mokutil"
    )
    
    print_info "Installing: ${DEPENDENCIES[*]}"
    sudo apt install -y "${DEPENDENCIES[@]}" >> "$LOG_FILE" 2>&1
    check_success "Dependencies installed" "Failed to install dependencies"
}

#############################################################################
# Download driver
#############################################################################

download_driver() {
    print_step "Downloading AIC8800D80 driver..."
    
    # If the directory already exists, ask if you want to update
    if [ -d "$DRIVER_DIR" ]; then
        print_warning "Directory $DRIVER_DIR already exists."
        read -p "Do you want to remove and download again? (y/N): " choice
        if [[ "$choice" =~ ^[Ss]$ ]]; then
            print_info "Removing old directory..."
            rm -rf "$DRIVER_DIR"
            check_success "Old directory removed" "Failed to remove old directory"
        else
            print_info "Using existing directory."
            cd "$DRIVER_DIR" || exit 1
            git pull >> "$LOG_FILE" 2>&1
            print_info "Repository updated."
            return 0
        fi
    fi
    
    # Clone repository
    print_info "Cloning repository..."
    cd "$HOME" || exit 1
    git clone "$DRIVER_REPO" >> "$LOG_FILE" 2>&1
    check_success "Repository cloned successfully" "Failed to clone repository" || exit 1
    
    cd "$DRIVER_DIR" || exit 1
}

#############################################################################
# Installation of driver
#############################################################################

install_driver() {
    print_step "Installing AIC8800D80 driver..."
    
    cd "$DRIVER_DIR" || { print_error "Driver directory not found!"; exit 1; }
    
    # Step 1: Copy udev rules
    print_info "Copying udev rules..."
    sudo cp aic.rules /lib/udev/rules.d/ >> "$LOG_FILE" 2>&1
    check_success "Udev rules copied" "Failed to copy udev rules"
    
    # Step 2: Remove old firmwares (IMPORTANT!)
    print_info "Removing old firmwares (prevents freezes)..."
    sudo rm -rf /lib/firmware/aic8800* >> "$LOG_FILE" 2>&1
    print_success "Old firmwares removed"
    
    # Step 3: Copy new firmware
    print_info "Copying new firmware AIC8800D80..."
    sudo cp -r ./fw/aic8800D80 /lib/firmware/ >> "$LOG_FILE" 2>&1
    check_success "Firmware copied" "Failed to copy firmware" || exit 1
    
    # Step 4: Navigate to driver directory
    print_info "Navigating to driver directory..."
    cd ./drivers/aic8800 || { print_error "Driver directory drivers/aic8800 not found!"; exit 1; }
    
    # Check if Makefile exists
    if [ ! -f "Makefile" ]; then
        print_error "Makefile not found in $(pwd)"
        exit 1
    fi
    
    # Step 5: Compile driver
    print_info "Compiling driver (this may take a few minutes)..."
    make >> "$LOG_FILE" 2>&1
    
    if [ $? -ne 0 ]; then
        print_warning "Compilation failed with default GCC. Trying with gcc-12..."
        make clean >> "$LOG_FILE" 2>&1
        make CC=gcc-12 >> "$LOG_FILE" 2>&1
        check_success "Driver compiled with gcc-12" "Failed to compile driver" || exit 1
    else
        print_success "Driver compiled successfully"
    fi
    
    # Step 6: Install driver
    print_info "Installing driver..."
    sudo make install >> "$LOG_FILE" 2>&1
    check_success "Driver installed" "Failed to install driver" || exit 1
}

#############################################################################
# Configure automatic loading
#############################################################################

configure_autoload() {
    print_step "Configuring automatic loading on boot..."
    
    # Add module to /etc/modules
    if ! grep -q "^$MODULE_NAME$" /etc/modules 2>/dev/null; then
        echo "$MODULE_NAME" | sudo tee -a /etc/modules >> "$LOG_FILE" 2>&1
        print_success "Module added to /etc/modules"
    else
        print_info "Module already configured in /etc/modules"
    fi
    
    # Create configuration file modules-load.d
    echo "$MODULE_NAME" | sudo tee /etc/modules-load.d/aic8800.conf >> "$LOG_FILE" 2>&1
    print_success "Configuration file created in /etc/modules-load.d/"
}

#############################################################################
# Load module
#############################################################################

load_module() {
    print_step "Loading driver module..."
    
    # Unload module if already loaded
    if lsmod | grep -q "$MODULE_NAME"; then
        print_info "Module already loaded. Reloading..."
        sudo modprobe -r "$MODULE_NAME" >> "$LOG_FILE" 2>&1
    fi
    
    # Load module
    sudo modprobe "$MODULE_NAME" >> "$LOG_FILE" 2>&1
    
    if [ $? -eq 0 ]; then
        print_success "Module loaded successfully"
        
        # Check if it is listed
        sleep 2
        if lsmod | grep -q "$MODULE_NAME"; then
            print_success "Module active in kernel"
            lsmod | grep aic | tee -a "$LOG_FILE"
        else
            print_warning "Module not appears in lsmod"
        fi
    else
        print_error "Failed to load module"
        echo "Check dmesg for more details: sudo dmesg | tail -50"
        return 1
    fi
}

#############################################################################
# Post-installation verification
#############################################################################

verify_installation() {
    print_step "Verifying installation..."
    
    # Check module
    if lsmod | grep -q "$MODULE_NAME"; then
        print_success "Module $MODULE_NAME is loaded"
    else
        print_warning "Module is not loaded"
    fi
    
    # Check firmware
    if [ -d "/lib/firmware/aic8800D80" ]; then
        print_success "Firmware installed in /lib/firmware/aic8800D80"
    else
        print_error "Firmware not found!"
    fi
    
    # Check wireless interfaces
    print_info "Wireless interfaces available:"
    iwconfig 2>/dev/null | grep -E "wlan|IEEE" | tee -a "$LOG_FILE"
    
    # Check USB devices
    print_info "USB devices AIC detected:"
    lsusb | grep -i "aic\|368B" | tee -a "$LOG_FILE"
}

#############################################################################
# Final instructions
#############################################################################

show_final_instructions() {
    echo ""
    echo -e "${GREEN}╔════════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║            INSTALLATION COMPLETED SUCCESSFULLY!                    ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "${CYAN}Next steps:${NC}"
    echo ""
    echo "1. ${YELLOW}CONNECT THE USB ADAPTER${NC} physically (if not already connected)"
    echo ""
    echo "2. Wait a few seconds and check if it was detected:"
    echo "   ${BLUE}iwconfig${NC}"
    echo "   ${BLUE}ip link show${NC}"
    echo ""
    echo "3. To view kernel logs about the device:"
    echo "   ${BLUE}sudo dmesg | tail -30${NC}"
    echo ""
    echo "4. To connect to a WiFi network:"
    echo "   ${BLUE}nmcli device wifi list${NC}"
    echo "   ${BLUE}nmcli device wifi connect \"NETWORK_NAME\" password \"PASSWORD\"${NC}"
    echo ""
    echo "5. To disable internal WiFi (optional):"
    echo "   ${BLUE}rfkill list${NC}"
    echo "   ${BLUE}sudo rfkill block <internal_wifi_number>${NC}"
    echo ""
    echo -e "${YELLOW}Important notes:${NC}"
    echo "- Bluetooth does not work with this driver (known limitation)"
    echo "- After kernel update, recompile the driver:"
    echo "  ${BLUE}cd $DRIVER_DIR/drivers/aic8800${NC}"
    echo "  ${BLUE}make clean && make && sudo make install${NC}"
    echo ""
    echo -e "${CYAN}Complete log saved in:${NC} $LOG_FILE"
    echo ""
}

#############################################################################
# Main function
#############################################################################

main() {
    # Clear previous log
    > "$LOG_FILE"
    
    echo -e "${CYAN}"
    echo "╔════════════════════════════════════════════════════════════════════╗"
    echo "║       Installation script for AIC8800D80 WiFi 6 driver             ║"
    echo "║                  Version 1.0 - 2025-12-29                          ║"
    echo "╚════════════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
    
    # Step 1: Check Secure Boot (REQUIRED)
    check_secure_boot || exit 1
    
    # Step 2: Check requirements
    check_requirements
    
    # Step 3: Install dependencies
    install_dependencies
    
    # Step 4: Download driver
    download_driver
    
    # Step 5: Install driver
    install_driver
    
    # Step 6: Configure autoload
    configure_autoload
    
    # Step 7: Load module
    load_module
    
    # Step 8: Verify installation
    verify_installation
    
    # Step 9: Show final instructions
    show_final_instructions
}

# Execute script
main "$@"

exit 0
