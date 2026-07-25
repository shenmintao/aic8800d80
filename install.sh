#!/bin/bash

#############################################################################
# AIC8800D80 WiFi 6 Driver - Universal Installer (DKMS) - VERSÃO CORRIGIDA
# Version: 2.0.4
# Date: 2025-12-30
# Description: Multi-distribution installer with DKMS support
# Supported: Debian/Ubuntu, Fedora/RHEL, Arch Linux, and derivatives
# FIX: Corrected DKMS module configuration for aic8800_fdrv
#############################################################################

set -e  # Exit on error

# Color definitions
readonly RED='\033[0;31m'
readonly GREEN='\033[0;32m'
readonly YELLOW='\033[1;33m'
readonly BLUE='\033[0;34m'
readonly CYAN='\033[0;36m'
readonly NC='\033[0m' # No Color

# Driver configuration
readonly DRV_NAME="aic8800"
readonly DRV_VERSION="1.0.0"
readonly SRC_DIR="/usr/src/${DRV_NAME}-${DRV_VERSION}"
readonly MODULE_NAME="aic8800_fdrv"
readonly LOG_FILE="/tmp/aic8800d80_install.log"

# Script directory (where the script is located)
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

#############################################################################
# Logging and Output Functions
#############################################################################

log_message() {
    local level="$1"
    shift
    # Garantir que o arquivo de log existe e tem permissões corretas
    if [ ! -f "$LOG_FILE" ]; then
        touch "$LOG_FILE" 2>/dev/null || true
        chmod 666 "$LOG_FILE" 2>/dev/null || true
    fi
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [$level] $*" >> "$LOG_FILE" 2>/dev/null || true
}

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
    log_message "INFO" "$1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
    log_message "SUCCESS" "$1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
    log_message "WARNING" "$1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
    log_message "ERROR" "$1"
}

print_step() {
    echo ""
    echo -e "${CYAN}==>${NC} ${1}"
    log_message "STEP" "$1"
}

run_logged() {
    local description="$1"
    shift
    local command_status=0

    "$@" 2>&1 | tee -a "$LOG_FILE"
    command_status="${PIPESTATUS[0]}"
    if [ "$command_status" -ne 0 ]; then
        print_error "$description failed (exit code $command_status)."
    fi

    return "$command_status"
}

#############################################################################
# Error Handling
#############################################################################

cleanup_on_error() {
    local exit_code=$?
    trap - ERR
    print_error "Installation failed (exit code $exit_code). Check $LOG_FILE for details."
    exit "$exit_code"
}

trap cleanup_on_error ERR

#############################################################################
# Root Privilege Check
#############################################################################

check_root() {
    if [ "$EUID" -ne 0 ]; then
        print_error "This script must be run as root (use sudo)"
        echo ""
        echo "Usage: sudo ./install.sh"
        exit 1
    fi
}

#############################################################################
# Secure Boot Detection
#############################################################################

check_secure_boot() {
    print_step "Checking Secure Boot status..."
    
    local secure_boot_enabled=false
    
    # Method 1: Check via mokutil
    if command -v mokutil &> /dev/null; then
        if mokutil --sb-state 2>/dev/null | grep -qi "SecureBoot enabled"; then
            secure_boot_enabled=true
        fi
    fi
    
    # Method 2: Check via EFI variables (fallback)
    if [ "$secure_boot_enabled" = false ] && [ -d /sys/firmware/efi ]; then
        local secureboot_files=(/sys/firmware/efi/efivars/SecureBoot-*)
        if [ -f "${secureboot_files[0]}" ]; then
            local sb_value
            sb_value=$(od -An -t u1 "${secureboot_files[0]}" 2>/dev/null | awk '{print $NF}')
            if [ "$sb_value" = "1" ]; then
                secure_boot_enabled=true
            fi
        fi
    fi
    
    if [ "$secure_boot_enabled" = true ]; then
        echo ""
        echo -e "${YELLOW}╔════════════════════════════════════════════════════════════════╗${NC}"
        echo -e "${YELLOW}║                  SECURE BOOT IS ENABLED                        ║${NC}"
        echo -e "${YELLOW}╚════════════════════════════════════════════════════════════════╝${NC}"
        echo ""
        echo "The driver may not load automatically due to Secure Boot restrictions."
        echo "Third-party modules require signing or Secure Boot must be disabled."
        echo ""
        echo -e "${CYAN}Options:${NC}"
        echo "  1. Disable Secure Boot in BIOS/UEFI settings (recommended)"
        echo "  2. Sign the module manually (advanced)"
        echo "  3. Continue installation anyway (module may fail to load)"
        echo ""
        read -p "Continue installation? (y/N): " choice
        if [[ ! "$choice" =~ ^[Yy]$ ]]; then
            print_info "Installation cancelled by user."
            exit 0
        fi
        print_warning "Continuing with Secure Boot enabled..."
    else
        print_success "Secure Boot is disabled or not present."
    fi
}

#############################################################################
# Package Manager Detection and Dependency Installation
#############################################################################

detect_package_manager() {
    print_step "Detecting package manager and distribution..."
    
    local pkg_manager=""
    local distro_name=""
    
    if command -v apt-get &> /dev/null; then
        pkg_manager="apt"
        distro_name="Debian/Ubuntu"
    elif command -v dnf &> /dev/null; then
        pkg_manager="dnf"
        distro_name="Fedora/RHEL"
    elif command -v yum &> /dev/null; then
        pkg_manager="yum"
        distro_name="CentOS/RHEL (legacy)"
    elif command -v pacman &> /dev/null; then
        pkg_manager="pacman"
        distro_name="Arch Linux"
    elif command -v zypper &> /dev/null; then
        pkg_manager="zypper"
        distro_name="openSUSE"
    else
        print_error "No supported package manager found!"
        echo ""
        echo "Please install the following packages manually:"
        echo "  - dkms"
        echo "  - build-essential / base-devel / development tools"
        echo "  - linux-headers for your kernel version"
        echo "  - mokutil (optional, for Secure Boot detection)"
        echo "  - eject"
        echo "  - usb_modeswitch / usb-modeswitch"
        echo ""
        exit 1
    fi
    
    print_success "Detected: $distro_name ($pkg_manager)"
    DETECTED_PKG_MANAGER="$pkg_manager"
}

is_volumio() {
    if [ -r /etc/os-release ] && grep -Eiq '(^ID="?volumio"?$|^NAME="?volumio"?$|^PRETTY_NAME=.*volumio)' /etc/os-release; then
        return 0
    fi

    return 1
}

kernel_build_tree_available() {
    local kernel_build_dir="/lib/modules/$(uname -r)/build"

    [ -d "$kernel_build_dir" ] && [ -r "$kernel_build_dir/Makefile" ]
}

install_dependencies() {
    local pkg_manager="$1"
    local install_kernel_headers=true
    
    print_step "Installing dependencies..."

    if kernel_build_tree_available; then
        install_kernel_headers=false
        print_info "Kernel build tree already exists; skipping generic kernel headers package."
    elif is_volumio; then
        install_kernel_headers=false
        print_info "Volumio detected; skipping generic kernel headers package."
    fi
    
    case "$pkg_manager" in
        apt)
            print_info "Updating package database..."
            run_logged "Package database update" apt-get update

            local -a packages=(dkms build-essential mokutil eject usb-modeswitch)
            if [ "$install_kernel_headers" = true ]; then
                packages+=("linux-headers-$(uname -r)")
            fi
            print_info "Installing: ${packages[*]}..."
            run_logged "Dependency installation" env DEBIAN_FRONTEND=noninteractive apt-get install -y "${packages[@]}"
            ;;
            
        dnf)
            local -a packages=(dkms make gcc mokutil util-linux usb_modeswitch)
            if [ "$install_kernel_headers" = true ]; then
                packages+=(kernel-devel kernel-headers)
            fi
            print_info "Installing: ${packages[*]}..."
            run_logged "Dependency installation" dnf install -y "${packages[@]}"
            ;;
            
        yum)
            local -a packages=(dkms make gcc mokutil util-linux usb_modeswitch)
            if [ "$install_kernel_headers" = true ]; then
                packages+=(kernel-devel)
            fi
            print_info "Enabling EPEL repository..."
            run_logged "EPEL repository installation" yum install -y epel-release
            print_info "Installing: ${packages[*]}..."
            run_logged "Dependency installation" yum install -y "${packages[@]}"
            ;;
            
        pacman)
            print_info "Syncing package database..."
            run_logged "Package database sync" pacman -Sy --noconfirm

            local -a packages=(dkms base-devel mokutil util-linux usb_modeswitch)
            if [ "$install_kernel_headers" = true ]; then
                packages+=(linux-headers)
            fi
            print_info "Installing: ${packages[*]}..."
            run_logged "Dependency installation" pacman -S --needed --noconfirm "${packages[@]}"
            ;;
            
        zypper)
            local -a packages=(dkms make gcc mokutil util-linux usb_modeswitch)
            if [ "$install_kernel_headers" = true ]; then
                packages+=(kernel-devel)
            fi
            print_info "Installing: ${packages[*]}..."
            run_logged "Dependency installation" zypper --non-interactive install "${packages[@]}"
            ;;
            
        *)
            print_error "Unsupported package manager: $pkg_manager"
            exit 1
            ;;
    esac
    
    print_success "Dependencies installed successfully."
}

check_kernel_build_tree() {
    local kernel_build_dir="/lib/modules/$(uname -r)/build"

    if kernel_build_tree_available; then
        return 0
    fi

    print_error "Kernel build directory is missing or incomplete: $kernel_build_dir"
    echo ""
    if is_volumio; then
        echo "Volumio does not use the normal linux-headers package flow."
        echo "Prepare the kernel source first, then rerun this installer:"
        echo ""
        echo "  sudo volumio kernelsource"
        echo "  sudo ./install.sh"
    else
        echo "Install the kernel headers for your running kernel, then rerun this installer."
    fi
    echo ""
    exit 1
}

#############################################################################
# Firmware Installation
#############################################################################

cleanup_legacy_bluetooth_config() {
    local legacy_conf="/etc/modprobe.d/aic8800-bt.conf"
    local legacy_udev="/etc/udev/rules.d/90-aic8800-mode-switch.rules"
    local legacy_package
    local changed=false

    if [ -f "$legacy_conf" ] && grep -Eq '^[[:space:]]*(softdep|alias)[^#]*aic_btusb([[:space:]]|$)' "$legacy_conf"; then
        print_info "Removing obsolete aic_btusb directives from $legacy_conf..."
        if [ ! -f "${legacy_conf}.aic8800-backup" ]; then
            cp -a "$legacy_conf" "${legacy_conf}.aic8800-backup" >> "$LOG_FILE" 2>&1
        fi
        sed -i -E '/^[[:space:]]*(softdep|alias)[^#]*aic_btusb([[:space:]]|$)/d' "$legacy_conf"
        if ! grep -Eq '^[[:space:]]*[^#[:space:]]' "$legacy_conf"; then
            rm -f "$legacy_conf"
        fi
        changed=true
    fi

    if [ -f "$legacy_udev" ] && grep -q 'aic_btusb/new_id' "$legacy_udev"; then
        print_info "Removing obsolete aic_btusb binding rules from $legacy_udev..."
        if [ ! -f "${legacy_udev}.aic8800-backup" ]; then
            cp -a "$legacy_udev" "${legacy_udev}.aic8800-backup" >> "$LOG_FILE" 2>&1
        fi
        sed -i '/aic_btusb\/new_id/d' "$legacy_udev"
        changed=true
    fi

    if lsmod | awk '{print $1}' | grep -qx 'aic_btusb'; then
        print_info "Unloading obsolete aic_btusb module..."
        if modprobe -r aic_btusb >> "$LOG_FILE" 2>&1; then
            changed=true
        else
            print_warning "aic_btusb is still in use; reboot or replug the adapter after installation."
        fi
    fi

    # Remove the two issue #63 diagnostic DKMS packages before the production
    # quirk is installed as part of aic8800/1.0.0. A patched btusb may remain
    # active in memory until reboot, but DKMS restores the distribution module
    # on disk when its test package is removed.
    for legacy_package in "btusb-aic-zlp/0.1" "aic-zlp-quirk/0.1"; do
        if dkms status 2>/dev/null | grep -q "^${legacy_package},"; then
            print_info "Removing superseded test package ${legacy_package}..."
            if dkms remove "$legacy_package" --all >> "$LOG_FILE" 2>&1; then
                changed=true
            else
                print_warning "Could not remove ${legacy_package}; remove it manually before reboot."
            fi
        fi
    done

    if [ "$changed" = true ]; then
        print_success "Legacy Bluetooth configuration cleaned up."
    fi
}

install_firmware() {
    print_step "Installing firmware..."

    local fw_base="${SCRIPT_DIR}/fw"

    if [ ! -d "$fw_base" ]; then
        print_error "Firmware directory not found: $fw_base"
        echo "Please ensure you're running the script from the repository root."
        exit 1
    fi

    cleanup_legacy_bluetooth_config

    # Remove old firmware versions
    if [ -d "/lib/firmware" ] && [ -n "$(find /lib/firmware -maxdepth 1 -name 'aic8800*' -type d 2>/dev/null)" ]; then
        print_info "Removing existing firmware..."
        rm -rf /lib/firmware/aic8800* >> "$LOG_FILE" 2>&1
    fi

    # Copy all firmware variants
    print_info "Installing firmware for all chip variants..."
    for fw_dir in "$fw_base"/aic8800*; do
        if [ -d "$fw_dir" ]; then
            local fw_name=$(basename "$fw_dir")
            local fw_dest="/lib/firmware/$fw_name"
            print_info "Copying $fw_name firmware to $fw_dest..."
            cp -r "$fw_dir" "$fw_dest" >> "$LOG_FILE" 2>&1
        fi
    done

    print_success "Firmware installed for all supported chip variants."

    # Install udev rules
    local rules_source="${SCRIPT_DIR}/aic.rules"
    local rules_dest="/usr/lib/udev/rules.d/aic.rules"

    if [ -f "$rules_source" ]; then
        print_info "Installing udev rules to $rules_dest..."
        cp "$rules_source" "$rules_dest" >> "$LOG_FILE" 2>&1
        # Reload udev rules
        udevadm control --reload-rules >> "$LOG_FILE" 2>&1 || true
        udevadm trigger >> "$LOG_FILE" 2>&1 || true
        print_success "Udev rules installed successfully."
    else
        print_warning "Udev rules file not found: $rules_source"
    fi

    # Install usb_modeswitch configuration for AIC8800D80 "Pandora" clone
    local modeswitch_source="${SCRIPT_DIR}/usb_modeswitch/1111_1111"
    local modeswitch_dest="/etc/usb_modeswitch.d/1111:1111"

    if [ -f "$modeswitch_source" ]; then
        print_info "Installing usb_modeswitch configuration to $modeswitch_dest..."
        mkdir -p /etc/usb_modeswitch.d >> "$LOG_FILE" 2>&1 || true
        cp "$modeswitch_source" "$modeswitch_dest" >> "$LOG_FILE" 2>&1
        print_success "USB modeswitch configuration installed successfully."
    else
        print_warning "USB modeswitch configuration file not found: $modeswitch_source"
    fi

    print_success "Firmware installed successfully."
}

#############################################################################
# DKMS Configuration - CORRIGIDO v2.0.4
#############################################################################

create_dkms_conf() {
    print_step "Configuring DKMS..."
    
    local dkms_conf="${SCRIPT_DIR}/dkms.conf"
    
    # Check if dkms.conf already exists
    if [ -f "$dkms_conf" ]; then
        print_info "DKMS configuration file already exists."
        return 0
    fi
    
    print_info "Creating dkms.conf..."
    
    # CORRIGIDO: Configuração para múltiplos módulos
    cat > "$dkms_conf" << EOF
PACKAGE_NAME="${DRV_NAME}"
PACKAGE_VERSION="${DRV_VERSION}"
CLEAN="cd drivers/aic8800 && make clean"
MAKE="cd drivers/aic8800 && make"

# Módulo principal (aic8800_fdrv)
BUILT_MODULE_NAME[0]="aic8800_fdrv"
BUILT_MODULE_LOCATION[0]="drivers/aic8800/aic8800_fdrv"
DEST_MODULE_LOCATION[0]="/updates/dkms"

# Módulo de carregamento de firmware (aic_load_fw)
BUILT_MODULE_NAME[1]="aic_load_fw"
BUILT_MODULE_LOCATION[1]="drivers/aic8800/aic_load_fw"
DEST_MODULE_LOCATION[1]="/updates/dkms"

# Quirk ZLP para Bluetooth ACL bulk TX (somente 368b:8d81)
BUILT_MODULE_NAME[2]="aic_zlp_quirk"
BUILT_MODULE_LOCATION[2]="drivers/aic8800/aic_zlp_quirk"
DEST_MODULE_LOCATION[2]="/updates/dkms"

AUTOINSTALL="yes"
EOF
    
    print_success "DKMS configuration created."
}

#############################################################################
# DKMS Installation
#############################################################################

install_via_dkms() {
    print_step "Installing driver via DKMS..."
    
    # Remove existing DKMS installation if present
    if dkms status | grep -q "${DRV_NAME}/${DRV_VERSION}"; then
        print_info "Removing existing DKMS installation..."
        dkms remove "${DRV_NAME}/${DRV_VERSION}" --all >> "$LOG_FILE" 2>&1 || true
    fi
    
    # Clean up old source directory
    if [ -n "$SRC_DIR" ] && [ -d "$SRC_DIR" ]; then
        print_info "Cleaning up old source directory..."
        rm -rf "$SRC_DIR"
    fi
    
    # Copy source to /usr/src
    print_info "Copying source files to $SRC_DIR..."
    mkdir -p "$SRC_DIR"
    cp -r "${SCRIPT_DIR}"/* "$SRC_DIR/" >> "$LOG_FILE" 2>&1
    
    # Add to DKMS
    print_info "Adding module to DKMS..."
    dkms add -m "${DRV_NAME}" -v "${DRV_VERSION}" >> "$LOG_FILE" 2>&1
    
    # Build with DKMS
    print_info "Building module (this may take a few minutes)..."
    if ! dkms build -m "${DRV_NAME}" -v "${DRV_VERSION}" >> "$LOG_FILE" 2>&1; then
        print_error "DKMS build failed!"
        echo ""
        echo "Please check the log file: $LOG_FILE"
        echo "Common issues:"
        echo "  - Missing kernel headers"
        echo "  - Compiler version mismatch"
        echo "  - Kernel version too new/old"
        exit 1
    fi
    
    print_success "Module built successfully."
    
    # Install with DKMS
    print_info "Installing module..."
    if ! dkms install -m "${DRV_NAME}" -v "${DRV_VERSION}" >> "$LOG_FILE" 2>&1; then
        if grep -qi "override by specifying --force" "$LOG_FILE"; then
            print_warning "Existing AIC8800 modules found for this kernel; retrying DKMS install with --force..."
            dkms install -m "${DRV_NAME}" -v "${DRV_VERSION}" --force >> "$LOG_FILE" 2>&1
        else
            print_error "DKMS install failed!"
            echo ""
            echo "Please check the log file: $LOG_FILE"
            exit 1
        fi
    fi
    
    print_success "Module installed via DKMS."
    print_info "The driver will automatically rebuild after kernel updates."
}

#############################################################################
# Initramfs Refresh
#############################################################################

refresh_initramfs() {
    print_step "Refreshing initramfs..."

    if is_volumio; then
        print_info "Volumio detected; skipping automatic initramfs refresh."
        return 0
    fi

    if command -v update-initramfs > /dev/null 2>&1; then
        print_info "Running update-initramfs for the current kernel..."
        if update-initramfs -u -k "$(uname -r)" >> "$LOG_FILE" 2>&1; then
            print_success "Initramfs refreshed successfully."
        else
            print_warning "update-initramfs failed; firmware may not be available during early boot."
        fi
    elif command -v dracut > /dev/null 2>&1; then
        print_info "Running dracut for the current kernel..."
        if dracut -f >> "$LOG_FILE" 2>&1; then
            print_success "Initramfs refreshed successfully."
        else
            print_warning "dracut failed; firmware may not be available during early boot."
        fi
    elif command -v mkinitcpio > /dev/null 2>&1; then
        print_info "Running mkinitcpio presets..."
        if mkinitcpio -P >> "$LOG_FILE" 2>&1; then
            print_success "Initramfs refreshed successfully."
        else
            print_warning "mkinitcpio failed; firmware may not be available during early boot."
        fi
    else
        print_info "No supported initramfs refresh tool found; skipping."
    fi
}

#############################################################################
# Module Loading
#############################################################################

aic_zlp_target_present() {
    local device

    for device in /sys/bus/usb/devices/*; do
        [ -r "$device/idVendor" ] || continue
        [ -r "$device/idProduct" ] || continue
        [ "$(cat "$device/idVendor")" = "368b" ] || continue
        [ "$(cat "$device/idProduct")" = "8d81" ] || continue
        return 0
    done

    return 1
}

load_module() {
    print_step "Loading kernel modules..."
    
    # Update module dependencies
    print_info "Updating module dependencies..."
    depmod -a >> "$LOG_FILE" 2>&1
    
    if lsmod | grep -q "$MODULE_NAME"; then
        print_info "Wi-Fi module already loaded. Reloading..."
        modprobe -r "$MODULE_NAME" >> "$LOG_FILE" 2>&1 || true
    fi
    
    # aic8800_fdrv depends on aic_load_fw, which initializes both Wi-Fi-only
    # adapters and the Bluetooth firmware on combo adapters. If a standard HCI
    # USB interface appears afterwards, the kernel USB modalias loads btusb.
    print_info "Loading $MODULE_NAME..."
    if modprobe "$MODULE_NAME" >> "$LOG_FILE" 2>&1; then
        print_success "Wi-Fi module loaded successfully."
        
        # Verify module is loaded
        print_info "Waiting for module to initialize..."
        local module_loaded=false
        for i in {1..10}; do
            if lsmod | grep -q "$MODULE_NAME"; then
                module_loaded=true
                break
            fi
            sleep 0.5
        done
        
        if [ "$module_loaded" = true ]; then
            print_success "Wi-Fi module is active in kernel."
        else
            print_warning "Wi-Fi module may not be fully initialized yet."
        fi
    else
        print_warning "Wi-Fi module installed but could not be loaded immediately."
        print_info "This may be due to Secure Boot or missing hardware."
        print_info "Try rebooting or check: sudo dmesg | grep aic8800"
    fi

    print_info "Combo adapters use the standard btusb driver when their Bluetooth interface appears."

    # The module has a USB alias and normally autoloads when 368b:8d81 appears.
    # Explicitly load it here as well when that device was already present before
    # DKMS installation and therefore did not generate a new modalias event.
    if aic_zlp_target_present; then
        print_info "Loading the 368b:8d81 Bluetooth ACL ZLP quirk..."
        if modprobe aic_zlp_quirk >> "$LOG_FILE" 2>&1; then
            print_success "Device-scoped Bluetooth ZLP quirk is active."
        else
            print_warning "The ZLP quirk could not attach; system btusb remains unchanged."
            print_info "Check CONFIG_KPROBES, Secure Boot, and: sudo dmesg | grep aic_zlp_quirk"
        fi
    fi
}

#############################################################################
# Post-Installation Verification
#############################################################################

verify_installation() {
    print_step "Verifying installation..."
    
    # Check DKMS status
    local dkms_status
    dkms_status=$(dkms status "${DRV_NAME}/${DRV_VERSION}" 2>/dev/null || echo "not found")
    
    if echo "$dkms_status" | grep -q "installed"; then
        print_success "DKMS module registered: $dkms_status"
    else
        print_warning "DKMS status unclear: $dkms_status"
    fi
    
    # Check if the Wi-Fi module is loaded
    if lsmod | grep -q "$MODULE_NAME"; then
        print_success "Wi-Fi kernel module is loaded."
    else
        print_info "Wi-Fi module not currently loaded (this is OK if no hardware is connected)."
    fi

    if lsmod | awk '{print $1}' | grep -qx 'btusb' || compgen -G '/sys/class/bluetooth/hci*' > /dev/null; then
        print_success "A Bluetooth controller or the standard btusb module is present."
    else
        print_info "No Bluetooth controller is present (normal for Wi-Fi-only adapters or when no combo adapter is connected)."
    fi

    if aic_zlp_target_present; then
        if lsmod | awk '{print $1}' | grep -qx 'aic_zlp_quirk'; then
            local zlp_hook="unknown"
            if [ -r /sys/module/aic_zlp_quirk/parameters/hook ]; then
                zlp_hook=$(cat /sys/module/aic_zlp_quirk/parameters/hook)
            fi
            print_success "Bluetooth ACL ZLP quirk is loaded (hook: $zlp_hook)."
        else
            print_warning "USB device 368b:8d81 is present but aic_zlp_quirk is not loaded."
        fi
    else
        print_info "Bluetooth ZLP quirk is installed but inactive (no 368b:8d81 device present)."
    fi

    echo ""
    print_info "Loaded AIC modules:"
    lsmod | grep aic || echo "  (none)"
    
    # Check firmware
    local fw_count=0
    for fw_dir in /lib/firmware/aic8800*; do
        if [ -d "$fw_dir" ]; then
            ((fw_count++))
        fi
    done
    if [ $fw_count -gt 0 ]; then
        print_success "Firmware installed for $fw_count chip variant(s) in /lib/firmware/"
    else
        print_warning "No firmware found in /lib/firmware/"
    fi

    # Check for wireless interfaces
    print_info "Checking for wireless interfaces..."
    if command -v iwconfig &> /dev/null; then
        iwconfig 2>/dev/null | grep -E "wlan|IEEE" || echo "No wireless interfaces detected (hardware may not be connected)"
    fi

    print_info "Checking for Bluetooth interfaces on combo adapters..."
    if command -v bluetoothctl &> /dev/null; then
        bluetoothctl list 2>/dev/null || echo "No Bluetooth interfaces detected (normal for Wi-Fi-only adapters)"
    elif command -v hciconfig &> /dev/null; then
        hciconfig 2>/dev/null || echo "No Bluetooth interfaces detected (normal for Wi-Fi-only adapters)"
    else
        echo "Bluetooth tools not installed (install BlueZ to manage a combo adapter)"
    fi
}

#############################################################################
# Final Instructions
#############################################################################

show_final_instructions() {
    echo ""
    echo -e "${GREEN}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║         INSTALLATION COMPLETED SUCCESSFULLY!                   ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "${CYAN}Important Information:${NC}"
    echo ""
    echo "✓ Wi-Fi driver and firmware loader installed via DKMS"
    echo "✓ Combo adapters use the standard Linux btusb driver"
    echo "✓ Device-scoped Bluetooth ACL ZLP quirk installed for 368b:8d81"
    echo "✓ Automatic rebuild enabled for kernel updates"
    echo "✓ Firmware installed in /lib/firmware/"
    echo ""
    echo -e "${CYAN}Next Steps:${NC}"
    echo ""
    echo "1. Connect your AIC8800 USB Wi-Fi or Wi-Fi/Bluetooth adapter"
    echo ""
    echo "2. Check if the adapter is detected:"
    echo "   ${BLUE}lsusb | grep -i aic${NC}"
    echo "   ${BLUE}iwconfig${NC}"
    echo "   ${BLUE}ip link show${NC}"
    echo ""
    echo "3. View kernel messages about the driver:"
    echo "   ${BLUE}sudo dmesg | grep aic8800${NC}"
    echo "   ${BLUE}sudo dmesg | grep -iE 'bluetooth|btusb|hci'${NC}"
    echo ""
    echo "4. Connect to a WiFi network:"
    echo "   ${BLUE}nmcli device wifi list${NC}"
    echo "   ${BLUE}nmcli device wifi connect \"SSID\" password \"PASSWORD\"${NC}"
    echo ""
    echo "5. For a combo adapter, check Bluetooth:"
    echo "   ${BLUE}bluetoothctl list${NC}"
    echo "   ${BLUE}bluetoothctl scan on${NC}"
    echo ""
    echo -e "${CYAN}Troubleshooting:${NC}"
    echo ""
    echo "• Check DKMS status:"
    echo "  ${BLUE}dkms status${NC}"
    echo ""
    echo "• Check loaded modules:"
    echo "  ${BLUE}lsmod | grep -E 'aic|btusb'${NC}"
    echo "  ${BLUE}cat /sys/module/aic_zlp_quirk/parameters/{hook,injections}${NC}"
    echo ""
    echo "• Manually load the Wi-Fi module and firmware loader:"
    echo "  ${BLUE}sudo modprobe aic8800_fdrv${NC}"
    echo ""
    echo "• View detailed logs:"
    echo "  ${BLUE}cat $LOG_FILE${NC}"
    echo ""
    echo -e "${YELLOW}Known Limitations:${NC}"
    echo "• Secure Boot may prevent module loading (disable in BIOS if needed)"
    echo ""
    echo -e "${CYAN}Uninstallation:${NC}"
    echo "  ${BLUE}sudo dkms remove ${DRV_NAME}/${DRV_VERSION} --all${NC}"
    echo "  ${BLUE}sudo rm -rf /lib/firmware/aic8800*${NC}"
    echo ""
}

#############################################################################
# Main Installation Flow
#############################################################################

main() {
    # Initialize log file
    rm -f "$LOG_FILE" 2>/dev/null || true
    touch "$LOG_FILE" 2>/dev/null || true
    chmod 666 "$LOG_FILE" 2>/dev/null || true
    
    echo -e "${CYAN}"
    echo "╔════════════════════════════════════════════════════════════════╗"
    echo "║     AIC8800D80 WiFi 6 Driver - Universal Installer (DKMS)      ║"
    echo "║                   Version 2.0.4 (Fixed)                        ║"
    echo "╚════════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
    
    log_message "START" "Installation started"
    log_message "INFO" "Kernel: $(uname -r)"
    log_message "INFO" "Distribution: $(cat /etc/os-release 2>/dev/null | grep PRETTY_NAME | cut -d= -f2 | tr -d '"')"
    
    # Step 1: Check root privileges
    check_root
    
    # Step 2: Check Secure Boot
    check_secure_boot
    
    # Step 3: Detect package manager
    detect_package_manager
    
    # Step 4: Install dependencies
    install_dependencies "$DETECTED_PKG_MANAGER"

    # Step 5: Check kernel build tree
    check_kernel_build_tree

    # Step 6: Install firmware
    install_firmware
    
    # Step 7: Create DKMS configuration
    create_dkms_conf
    
    # Step 8: Install via DKMS
    install_via_dkms
    
    # Step 9: Refresh initramfs
    refresh_initramfs

    # Step 10: Load the module
    load_module
    
    # Step 11: Verify installation
    verify_installation
    
    # Step 12: Show final instructions
    show_final_instructions
    
    log_message "END" "Installation completed successfully"
}

# Execute main function
main "$@"

exit 0
