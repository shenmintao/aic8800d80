#!/bin/bash

#############################################################################
# AIC8800D80 WiFi 6 Driver - Universal Installer (DKMS) - VERSÃO CORRIGIDA
# Version: 2.0.4
# Date: 2025-12-30
# Description: Multi-distribution installer with DKMS support
# Supported: Debian/Ubuntu, Fedora/RHEL, Arch Linux, and derivatives
# FIX: Corrected DKMS module configuration for aic8800_fdrv
#############################################################################

set -eE  # Exit on error and run the ERR trap inside functions

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
readonly LOG_FILE="/var/log/aic8800-install.log"

# Script directory (where the script is located)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly SCRIPT_DIR

#############################################################################
# Logging and Output Functions
#############################################################################

log_message() {
    local level="$1"
    shift
    { echo "[$(date '+%Y-%m-%d %H:%M:%S')] [$level] $*" >> "$LOG_FILE"; } 2>/dev/null || true
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

# shellcheck disable=SC2317  # invoked indirectly through the ERR trap
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
        echo "With Secure Boot enabled the kernel only loads modules signed by a key"
        echo "it trusts. Out-of-tree modules built by DKMS are not signed by your"
        echo "distribution, so the driver will not load until you either:"
        echo ""
        echo -e "${CYAN}Options:${NC}"
        echo "  1. Enroll a Machine Owner Key (MOK) that DKMS signs with. Recent DKMS"
        echo "     versions create one and sign automatically; enroll it with"
        echo "     'sudo mokutil --import <key.der>' and confirm in the blue MOK"
        echo "     manager screen on the next boot. See your distribution's DKMS"
        echo "     documentation for where the key is stored."
        echo "  2. Disable Secure Boot in the firmware settings."
        echo "  3. Continue anyway; the module will be installed but will not load"
        echo "     until one of the above is done."
        echo ""
        local choice
        if [ ! -t 0 ]; then
            print_error "Secure Boot is enabled and no terminal is attached to answer the prompt; stopping."
            exit 1
        fi
        read -r -p "Continue installation? (y/N): " choice || choice="n"
        if [[ ! "$choice" =~ ^[Yy]$ ]]; then
            print_info "Installation cancelled by user. Nothing was changed."
            exit 1
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
    local kernel_build_dir
    kernel_build_dir="/lib/modules/$(uname -r)/build"

    [ -d "$kernel_build_dir" ] && [ -r "$kernel_build_dir/Makefile" ]
}

# Headers metapackages matching the installed kernel image metapackages:
# linux-image-amd64 -> linux-headers-amd64, linux-image-generic-hwe-24.04 ->
# linux-headers-generic-hwe-24.04. Ubuntu's GA and HWE kernels both end in
# "-generic", so the running kernel's release cannot tell the tracks apart;
# the installed metapackage can. Versioned and unsigned images are skipped,
# and so is any metapackage without a headers counterpart (-dbg).
kernel_headers_metapackages() {
    local status image
    dpkg-query -W -f='${db:Status-Abbrev} ${Package}\n' 'linux-image-*' 2>/dev/null |
        while read -r status image; do
            [ "$status" = ii ] || continue
            case "$image" in
                linux-image-[0-9]* | linux-image-unsigned-*) continue ;;
            esac
            if apt-cache show "linux-headers-${image#linux-image-}" > /dev/null 2>&1; then
                echo "linux-headers-${image#linux-image-}"
            fi
        done
}

# Headers that follow kernel upgrades. Without them DKMS has nothing to
# build against when a new kernel is installed and skips the rebuild
# silently, so the adapter stops working after the next reboot.
install_kernel_headers() {
    local pkg_manager="$1"
    local kver
    kver="$(uname -r)"
    local -a headers=() metapackages=()

    case "$pkg_manager" in
        apt)
            if ! kernel_build_tree_available; then
                headers+=("linux-headers-${kver}")
            fi
            mapfile -t metapackages < <(kernel_headers_metapackages)
            if [ "${#metapackages[@]}" -gt 0 ]; then
                headers+=("${metapackages[@]}")
            else
                print_warning "No kernel headers metapackage found for the installed kernel packages."
                print_warning "Install headers for each new kernel yourself, or DKMS cannot rebuild the driver after kernel updates."
            fi
            [ "${#headers[@]}" -eq 0 ] && return 0
            run_logged "Kernel headers installation" env DEBIAN_FRONTEND=noninteractive apt-get install -y "${headers[@]}"
            ;;
        dnf|yum)
            # kernel-devel is install-only and is upgraded alongside the kernel.
            headers=("kernel-devel-${kver}" kernel-devel)
            run_logged "Kernel headers installation" "$pkg_manager" install -y "${headers[@]}"
            ;;
        pacman)
            # The pkgbase file names the kernel package (linux, linux-lts, linux-zen, ...).
            local pkgbase="linux"
            if [ -r "/usr/lib/modules/${kver}/pkgbase" ]; then
                pkgbase="$(cat "/usr/lib/modules/${kver}/pkgbase")"
            fi
            headers=("${pkgbase}-headers")
            run_logged "Kernel headers installation" pacman -S --needed --noconfirm "${headers[@]}"
            ;;
        zypper)
            # openSUSE names headers kernel-<flavour>-devel (kernel-default-devel).
            headers=("kernel-${kver##*-}-devel")
            run_logged "Kernel headers installation" zypper --non-interactive install "${headers[@]}"
            ;;
    esac
}

install_dependencies() {
    local pkg_manager="$1"
    local install_kernel_headers=true

    print_step "Installing dependencies..."

    if is_volumio; then
        install_kernel_headers=false
        print_info "Volumio detected; skipping generic kernel headers package."
    fi

    case "$pkg_manager" in
        apt)
            print_info "Updating package database..."
            run_logged "Package database update" apt-get update

            local -a packages=(dkms build-essential mokutil eject usb-modeswitch)
            print_info "Installing: ${packages[*]}..."
            run_logged "Dependency installation" env DEBIAN_FRONTEND=noninteractive apt-get install -y "${packages[@]}"
            ;;

        dnf)
            local -a packages=(dkms make gcc mokutil util-linux usb_modeswitch)
            print_info "Installing: ${packages[*]}..."
            run_logged "Dependency installation" dnf install -y "${packages[@]}"
            ;;

        yum)
            local -a packages=(dkms make gcc mokutil util-linux usb_modeswitch)
            print_info "Enabling EPEL repository..."
            run_logged "EPEL repository installation" yum install -y epel-release
            print_info "Installing: ${packages[*]}..."
            run_logged "Dependency installation" yum install -y "${packages[@]}"
            ;;

        pacman)
            local -a packages=(dkms base-devel mokutil util-linux usb_modeswitch)
            print_info "Installing: ${packages[*]}..."
            if ! run_logged "Dependency installation" pacman -S --needed --noconfirm "${packages[@]}"; then
                print_error "Check the pacman errors above. If the package database is stale, run 'sudo pacman -Syu', reboot if the kernel was updated, then rerun this installer."
                return 1
            fi
            ;;

        zypper)
            local -a packages=(dkms make gcc mokutil util-linux usb_modeswitch)
            print_info "Installing: ${packages[*]}..."
            run_logged "Dependency installation" zypper --non-interactive install "${packages[@]}"
            ;;

        *)
            print_error "Unsupported package manager: $pkg_manager"
            exit 1
            ;;
    esac

    if [ "$install_kernel_headers" = true ]; then
        print_info "Installing kernel headers for $(uname -r) and future kernels..."
        if ! install_kernel_headers "$pkg_manager"; then
            # Not fatal here: check_kernel_build_tree stops the install if the
            # running kernel's build tree is still missing.
            print_warning "Kernel headers could not be installed automatically."
        fi
    fi

    print_success "Dependencies installed successfully."
}

check_kernel_build_tree() {
    local kernel_build_dir
    kernel_build_dir="/lib/modules/$(uname -r)/build"

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

# Print the package that owns a path, if any package manager claims it.
firmware_path_owner() {
    local path="$1" alt owner=""
    alt="/usr${path}"
    [[ "$path" == /usr/* ]] && alt="${path#/usr}"

    if command -v dpkg > /dev/null 2>&1; then
        owner=$(dpkg -S "$path" 2>/dev/null || dpkg -S "$alt" 2>/dev/null || true)
        owner="${owner%%:*}"
    fi
    if [ -z "$owner" ] && command -v rpm > /dev/null 2>&1; then
        owner=$(rpm -qf "$path" 2>/dev/null || true)
        [[ "$owner" == *"not owned"* ]] && owner=""
    fi
    if [ -z "$owner" ] && command -v pacman > /dev/null 2>&1; then
        owner=$(pacman -Qqo "$path" 2>/dev/null || true)
    fi
    printf '%s' "$owner"
}

# True if the installed firmware directory $1 holds exactly the files of the
# shipped directory $2. If diff is missing, or anything differs, it is false and
# the caller keeps a backup.
firmware_matches_shipped() {
    [ -d "$1" ] && [ -d "$2" ] && diff -rq "$1" "$2" > /dev/null 2>&1
}

install_firmware() {
    print_step "Installing firmware..."

    local fw_base="${SCRIPT_DIR}/fw"

    if [ ! -d "$fw_base" ]; then
        print_error "Firmware directory not found: $fw_base"
        echo "Please ensure you're running the script from the repository root."
        exit 1
    fi

    # Validate the source before changing any installed firmware or config.
    local -a fw_dirs=()
    local fw_dir
    for fw_dir in "$fw_base"/aic8800*; do
        [ -d "$fw_dir" ] || continue
        if [ -z "$(find "$fw_dir" -type f -name '*.bin' -print -quit)" ]; then
            print_error "No firmware binaries found in $fw_dir; existing firmware has not been changed."
            exit 1
        fi
        fw_dirs+=("$fw_dir")
    done
    if [ "${#fw_dirs[@]}" -eq 0 ]; then
        print_error "No firmware directories found in $fw_base; existing firmware has not been changed."
        exit 1
    fi

    # Never remove firmware that a distribution or AUR package installed;
    # deleting it would leave that package's file list out of sync.
    local -a existing_fw=()
    local existing owner
    mapfile -t existing_fw < <(find /lib/firmware -maxdepth 1 -name 'aic8800*' -type d 2>/dev/null | sort)
    for existing in "${existing_fw[@]}"; do
        owner=$(firmware_path_owner "$existing")
        if [ -n "$owner" ]; then
            print_error "$existing belongs to the package '$owner'; existing firmware has not been changed."
            echo "Remove that package first if you want this installer to manage the AIC8800 firmware."
            exit 1
        fi
    done

    cleanup_legacy_bluetooth_config

    # Firmware that is exactly what this repository ships needs no backup:
    # installing again restores it. Backing it up anyway would leave one more
    # identical copy in /var/backups on every reinstall.
    local -a to_backup=()
    for existing in "${existing_fw[@]}"; do
        if firmware_matches_shipped "$existing" "$fw_base/$(basename "$existing")"; then
            print_info "$(basename "$existing") already matches the shipped firmware; replacing it without a backup."
            rm -rf "${existing:?}"
        else
            to_backup+=("$existing")
        fi
    done

    # Move any other old firmware aside instead of deleting it. Mixing
    # firmware versions can hang the adapter, so the old directories must go,
    # but a backup lets the previous state be restored by hand.
    if [ "${#to_backup[@]}" -gt 0 ]; then
        local backup_root backup_dir
        local -a older_backups=()
        backup_root="${FW_BACKUP_ROOT:-/var/backups}"
        backup_dir="$backup_root/aic8800-firmware-install-$(date '+%Y%m%d-%H%M%S')"
        print_info "Moving existing firmware to $backup_dir..."
        mkdir -p "$backup_dir"
        mv "${to_backup[@]}" "$backup_dir/" >> "$LOG_FILE" 2>&1
        print_info "Previous firmware saved in $backup_dir."
        print_info "To restore it, replace each aic8800* directory in /lib/firmware with the one of the same name there."
        # Keep this backup and the two newest older ones made by this
        # installer (the names sort by date). Other aic8800-firmware-*
        # directories, from uninstall.sh or older installers, are left alone.
        mapfile -t older_backups < <(find "$backup_root" -mindepth 1 -maxdepth 1 -type d \
            -name 'aic8800-firmware-install-*' ! -path "$backup_dir" 2>/dev/null | sort)
        while [ "${#older_backups[@]}" -gt 2 ]; do
            print_info "Removing old firmware backup ${older_backups[0]}..."
            rm -rf "${older_backups[0]:?}"
            older_backups=("${older_backups[@]:1}")
        done
    fi

    # Copy all firmware variants (minimal systems may not have /lib/firmware yet)
    print_info "Installing firmware for all chip variants..."
    mkdir -p /lib/firmware
    for fw_dir in "${fw_dirs[@]}"; do
        if [ -d "$fw_dir" ]; then
            local fw_name
            fw_name=$(basename "$fw_dir")
            local fw_dest="/lib/firmware/$fw_name"
            print_info "Copying $fw_name firmware to $fw_dest..."
            cp -r "$fw_dir" "$fw_dest" >> "$LOG_FILE" 2>&1
        fi
    done

    print_success "Firmware installed for all supported chip variants."

    # Install udev rules
    local rules_source="${SCRIPT_DIR}/aic.rules"
    # /etc/udev/rules.d is the administrator's location; /usr may be
    # read-only (image-based distributions) and belongs to the package manager.
    local rules_dest="/etc/udev/rules.d/aic.rules"

    if [ -f "$rules_source" ]; then
        print_info "Installing udev rules to $rules_dest..."
        mkdir -p /etc/udev/rules.d
        cp "$rules_source" "$rules_dest" >> "$LOG_FILE" 2>&1
        # An aic.rules in /usr/lib or /lib (from an earlier installer, or from
        # a package) is shadowed by this one and is left alone.
        udevadm control --reload-rules >> "$LOG_FILE" 2>&1 || true
        # Re-run the rules only for a 1111:1111 adapter that is already
        # plugged in, rather than re-triggering every device on the system.
        udevadm trigger --action=add --subsystem-match=usb \
            --attr-match=idVendor=1111 --attr-match=idProduct=1111 >> "$LOG_FILE" 2>&1 || true
        print_success "Udev rules installed successfully."
        print_info "If an adapter in mass-storage mode is already plugged in, unplug and replug it."
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

require_dkms_conf() {
    print_step "Checking DKMS configuration..."

    # The shipped dkms.conf passes KVER/KDIR so DKMS builds for the kernel it
    # is targeting. An earlier fallback generated a dkms.conf without them,
    # which would build against the running kernel during autoinstall.
    if [ ! -f "${SCRIPT_DIR}/dkms.conf" ]; then
        print_error "dkms.conf not found in ${SCRIPT_DIR}; refusing to continue."
        echo "Run this script from a complete checkout of the repository."
        exit 1
    fi

    print_success "Using ${SCRIPT_DIR}/dkms.conf"
}

#############################################################################
# Pre-flight Build
#############################################################################

preflight_build() {
    local build_root kver
    kver="$(uname -r)"
    print_step "Test-building the driver for $kver before replacing the existing installation..."

    build_root=$(mktemp -d "${TMPDIR:-/tmp}/aic8800-preflight.XXXXXX")
    # Use a private copy and clean it so a previous build cannot hide an error.
    if cp -r "${SCRIPT_DIR}/drivers" "$build_root/" >> "$LOG_FILE" 2>&1 &&
            make -C "$build_root/drivers/aic8800" KVER="$kver" KDIR="/lib/modules/$kver/build" clean >> "$LOG_FILE" 2>&1 &&
            make -C "$build_root/drivers/aic8800" KVER="$kver" KDIR="/lib/modules/$kver/build" >> "$LOG_FILE" 2>&1; then
        rm -rf "$build_root"
        print_success "Test build succeeded."
    else
        rm -rf "$build_root"
        print_error "Test build failed for kernel $kver. Existing firmware and DKMS installation were not changed."
        echo "See $LOG_FILE for the compiler output."
        return 1
    fi
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

    # Record which source revision is installed. DKMS only knows the fixed
    # package version, so without this "dkms status" cannot tell installs apart.
    {
        echo "installed: $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
        echo "revision: $(git -c safe.directory="$SCRIPT_DIR" -C "$SCRIPT_DIR" describe --always --dirty --tags 2>/dev/null || echo unknown)"
    } > "$SRC_DIR/SOURCE_REVISION"
    
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
        for _ in {1..10}; do
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
    if [ -r "$SRC_DIR/SOURCE_REVISION" ]; then
        print_info "Installed source: $(tr '\n' ' ' < "$SRC_DIR/SOURCE_REVISION")"
    fi
    
    # Check if the Wi-Fi module is loaded, and that it is the one just
    # installed rather than an older copy that could not be unloaded.
    if lsmod | grep -q "$MODULE_NAME"; then
        local running_src installed_src
        running_src=$(cat "/sys/module/$MODULE_NAME/srcversion" 2>/dev/null || true)
        installed_src=$(modinfo -F srcversion "$MODULE_NAME" 2>/dev/null || true)
        if [ -n "$running_src" ] && [ -n "$installed_src" ] && [ "$running_src" != "$installed_src" ]; then
            print_warning "A different build of $MODULE_NAME is still loaded (the old one could not be unloaded)."
            print_warning "Reboot, or unplug the adapter and run: sudo modprobe -r $MODULE_NAME aic_load_fw && sudo modprobe $MODULE_NAME"
        else
            print_success "Wi-Fi kernel module is loaded."
        fi
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
            fw_count=$((fw_count + 1))
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
    echo -e "   ${BLUE}lsusb | grep -i aic${NC}"
    echo -e "   ${BLUE}iwconfig${NC}"
    echo -e "   ${BLUE}ip link show${NC}"
    echo ""
    echo "3. View kernel messages about the driver:"
    echo -e "   ${BLUE}sudo dmesg | grep aic8800${NC}"
    echo -e "   ${BLUE}sudo dmesg | grep -iE 'bluetooth|btusb|hci'${NC}"
    echo ""
    echo "4. Connect to a WiFi network:"
    echo -e "   ${BLUE}nmcli device wifi list${NC}"
    echo -e "   ${BLUE}nmcli device wifi connect \"SSID\" password \"PASSWORD\"${NC}"
    echo ""
    echo "5. For a combo adapter, check Bluetooth:"
    echo -e "   ${BLUE}bluetoothctl list${NC}"
    echo -e "   ${BLUE}bluetoothctl scan on${NC}"
    echo ""
    echo -e "${CYAN}Troubleshooting:${NC}"
    echo ""
    echo "• Check DKMS status:"
    echo -e "  ${BLUE}dkms status${NC}"
    echo ""
    echo "• Check loaded modules:"
    echo -e "  ${BLUE}lsmod | grep -E 'aic|btusb'${NC}"
    echo -e "  ${BLUE}cat /sys/module/aic_zlp_quirk/parameters/{hook,injections}${NC}"
    echo ""
    echo "• Manually load the Wi-Fi module and firmware loader:"
    echo -e "  ${BLUE}sudo modprobe aic8800_fdrv${NC}"
    echo ""
    echo "• View detailed logs:"
    echo -e "  ${BLUE}cat $LOG_FILE${NC}"
    echo ""
    echo -e "${YELLOW}Known Limitations:${NC}"
    echo "• With Secure Boot enabled, the module loads only after its signing key is enrolled (MOK)"
    echo ""
    echo -e "${CYAN}Uninstallation:${NC}"
    echo -e "  ${BLUE}sudo ./uninstall.sh --dry-run${NC}   (show what would be removed)"
    echo -e "  ${BLUE}sudo ./uninstall.sh${NC}"
    echo ""
}

#############################################################################
# Main Installation Flow
#############################################################################

main() {
    # Step 1: Check root privileges (before touching the log, which lives in
    # /var/log and is written by root only)
    check_root

    # Initialize log file: owned by root, readable by all, never world-writable
    rm -f "$LOG_FILE"
    install -m 0644 /dev/null "$LOG_FILE"

    echo -e "${CYAN}"
    echo "╔════════════════════════════════════════════════════════════════╗"
    echo "║     AIC8800D80 WiFi 6 Driver - Universal Installer (DKMS)      ║"
    echo "║                   Version 2.0.4 (Fixed)                        ║"
    echo "╚════════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
    
    log_message "START" "Installation started"
    log_message "INFO" "Kernel: $(uname -r)"
    log_message "INFO" "Distribution: $(grep PRETTY_NAME /etc/os-release 2>/dev/null | cut -d= -f2 | tr -d '"')"
    
    # Step 2: Check Secure Boot
    check_secure_boot
    
    # Step 3: Detect package manager
    detect_package_manager
    
    # Step 4: Install dependencies
    install_dependencies "$DETECTED_PKG_MANAGER"

    # Step 5: Check kernel build tree
    check_kernel_build_tree

    # Step 6: Check the DKMS configuration, then test-build before changing the system
    require_dkms_conf
    preflight_build

    # Step 7: Install firmware
    install_firmware
    
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
