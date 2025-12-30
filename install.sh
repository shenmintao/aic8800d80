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

#############################################################################
# Error Handling
#############################################################################

cleanup_on_error() {
    print_error "Installation failed. Check $LOG_FILE for details."
    exit 1
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
        echo ""
        exit 1
    fi
    
    print_success "Detected: $distro_name ($pkg_manager)"
    DETECTED_PKG_MANAGER="$pkg_manager"
}

install_dependencies() {
    local pkg_manager="$1"
    
    print_step "Installing dependencies..."
    
    case "$pkg_manager" in
        apt)
            print_info "Updating package database..."
            apt-get update -qq >> "$LOG_FILE" 2>&1
            
            print_info "Installing: dkms, build-essential, linux-headers, mokutil..."
            apt-get install -y dkms build-essential linux-headers-$(uname -r) mokutil >> "$LOG_FILE" 2>&1
            ;;
            
        dnf)
            print_info "Installing: dkms, gcc, make, kernel-devel, kernel-headers, mokutil..."
            dnf install -y dkms make gcc kernel-devel kernel-headers mokutil >> "$LOG_FILE" 2>&1
            ;;
            
        yum)
            print_info "Installing: dkms, gcc, make, kernel-devel, mokutil..."
            yum install -y epel-release >> "$LOG_FILE" 2>&1
            yum install -y dkms make gcc kernel-devel mokutil >> "$LOG_FILE" 2>&1
            ;;
            
        pacman)
            print_info "Syncing package database..."
            pacman -Sy --noconfirm >> "$LOG_FILE" 2>&1
            
            print_info "Installing: dkms, base-devel, linux-headers, mokutil..."
            pacman -S --noconfirm dkms base-devel linux-headers mokutil >> "$LOG_FILE" 2>&1
            ;;
            
        zypper)
            print_info "Installing: dkms, gcc, make, kernel-devel, mokutil..."
            zypper install -y dkms make gcc kernel-devel mokutil >> "$LOG_FILE" 2>&1
            ;;
            
        *)
            print_error "Unsupported package manager: $pkg_manager"
            exit 1
            ;;
    esac
    
    print_success "Dependencies installed successfully."
}

#############################################################################
# Firmware Installation
#############################################################################

install_firmware() {
    print_step "Installing firmware..."
    
    local fw_source="${SCRIPT_DIR}/fw/aic8800D80"
    local fw_dest="/lib/firmware/aic8800D80"
    
    if [ ! -d "$fw_source" ]; then
        print_error "Firmware directory not found: $fw_source"
        echo "Please ensure you're running the script from the repository root."
        exit 1
    fi
    
    # Remove old firmware versions
    if [ -d "$fw_dest" ]; then
        print_info "Removing existing firmware..."
        if [ -d "/lib/firmware" ] && [ -n "$(find /lib/firmware -maxdepth 1 -name 'aic8800*' -type d 2>/dev/null)" ]; then
            rm -rf /lib/firmware/aic8800* >> "$LOG_FILE" 2>&1
        fi
    fi
    
    # Copy new firmware
    print_info "Copying firmware to $fw_dest..."
    cp -r "$fw_source" "$fw_dest" >> "$LOG_FILE" 2>&1
    
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
    dkms install -m "${DRV_NAME}" -v "${DRV_VERSION}" >> "$LOG_FILE" 2>&1
    
    print_success "Module installed via DKMS."
    print_info "The driver will automatically rebuild after kernel updates."
}

#############################################################################
# Module Loading
#############################################################################

load_module() {
    print_step "Loading kernel module..."
    
    # Update module dependencies
    print_info "Updating module dependencies..."
    depmod -a >> "$LOG_FILE" 2>&1
    
    # Unload module if already loaded
    if lsmod | grep -q "$MODULE_NAME"; then
        print_info "Module already loaded. Reloading..."
        modprobe -r "$MODULE_NAME" >> "$LOG_FILE" 2>&1 || true
    fi
    
    # Load the module
    print_info "Loading $MODULE_NAME..."
    if modprobe "$MODULE_NAME" >> "$LOG_FILE" 2>&1; then
        print_success "Module loaded successfully."
        
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
            print_success "Module is active in kernel."
        else
            print_warning "Module may not be fully initialized yet."
        fi
    else
        print_warning "Module installed but could not be loaded immediately."
        print_info "This may be due to Secure Boot or missing hardware."
        print_info "Try rebooting or check: sudo dmesg | grep aic8800"
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
    
    # Check if module is loaded
    if lsmod | grep -q "$MODULE_NAME"; then
        print_success "Kernel module is loaded."
        echo ""
        lsmod | grep aic
    else
        print_info "Module not currently loaded (this is OK if no hardware is connected)."
    fi
    
    # Check firmware
    if [ -d "/lib/firmware/aic8800D80" ]; then
        print_success "Firmware installed in /lib/firmware/aic8800D80"
    fi
    
    # Check for wireless interfaces
    print_info "Checking for wireless interfaces..."
    if command -v iwconfig &> /dev/null; then
        iwconfig 2>/dev/null | grep -E "wlan|IEEE" || echo "No wireless interfaces detected (hardware may not be connected)"
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
    echo "✓ Driver installed via DKMS"
    echo "✓ Automatic rebuild enabled for kernel updates"
    echo "✓ Firmware installed in /lib/firmware/"
    echo ""
    echo -e "${CYAN}Next Steps:${NC}"
    echo ""
    echo "1. Connect your AIC8800D80 USB WiFi adapter"
    echo ""
    echo "2. Check if the adapter is detected:"
    echo "   ${BLUE}lsusb | grep -i aic${NC}"
    echo "   ${BLUE}iwconfig${NC}"
    echo "   ${BLUE}ip link show${NC}"
    echo ""
    echo "3. View kernel messages about the driver:"
    echo "   ${BLUE}sudo dmesg | grep aic8800${NC}"
    echo ""
    echo "4. Connect to a WiFi network:"
    echo "   ${BLUE}nmcli device wifi list${NC}"
    echo "   ${BLUE}nmcli device wifi connect \"SSID\" password \"PASSWORD\"${NC}"
    echo ""
    echo -e "${CYAN}Troubleshooting:${NC}"
    echo ""
    echo "• Check DKMS status:"
    echo "  ${BLUE}dkms status${NC}"
    echo ""
    echo "• Check loaded modules:"
    echo "  ${BLUE}lsmod | grep aic8800${NC}"
    echo ""
    echo "• Manually load the module:"
    echo "  ${BLUE}sudo modprobe aic8800_fdrv${NC}"
    echo ""
    echo "• View detailed logs:"
    echo "  ${BLUE}cat $LOG_FILE${NC}"
    echo ""
    echo -e "${YELLOW}Known Limitations:${NC}"
    echo "• Bluetooth functionality is not supported"
    echo "• Secure Boot may prevent module loading (disable in BIOS if needed)"
    echo ""
    echo -e "${CYAN}Uninstallation:${NC}"
    echo "  ${BLUE}sudo dkms remove ${DRV_NAME}/${DRV_VERSION} --all${NC}"
    echo "  ${BLUE}sudo rm -rf /lib/firmware/aic8800D80${NC}"
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
    echo "║     AIC8800D80 WiFi 6 Driver - Universal Installer (DKMS)     ║"
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
    
    # Step 5: Install firmware
    install_firmware
    
    # Step 6: Create DKMS configuration
    create_dkms_conf
    
    # Step 7: Install via DKMS
    install_via_dkms
    
    # Step 8: Load the module
    load_module
    
    # Step 9: Verify installation
    verify_installation
    
    # Step 10: Show final instructions
    show_final_instructions
    
    log_message "END" "Installation completed successfully"
}

# Execute main function
main "$@"

exit 0
