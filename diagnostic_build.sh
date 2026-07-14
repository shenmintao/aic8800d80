#!/bin/bash

#############################################################################
# Diagnostic Build Script - AIC8800D80 Driver Build Failure
# Run this script to diagnose the problem with the DKMS
#############################################################################

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║           DIAGNOSTIC BUILD SCRIPT - AIC8800D80 DRIVER          ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

DRV_NAME="aic8800"
DRV_VERSION="1.0.0"
INSTALL_LOG="/tmp/aic8800d80_install.log"

read_dkms_conf_value() {
    local key="$1"
    local file="$2"

    grep -E "^[[:space:]]*${key}[[:space:]]*=" "$file" 2>/dev/null \
        | head -1 \
        | cut -d= -f2- \
        | tr -d "\"'[:space:]"
}

if [ -f "./dkms.conf" ]; then
    detected_name="$(read_dkms_conf_value PACKAGE_NAME ./dkms.conf)"
    detected_version="$(read_dkms_conf_value PACKAGE_VERSION ./dkms.conf)"
    [ -n "$detected_name" ] && DRV_NAME="$detected_name"
    [ -n "$detected_version" ] && DRV_VERSION="$detected_version"
fi

mapfile -t DKMS_SOURCE_DIRS < <(find /usr/src -maxdepth 1 -type d -name "${DRV_NAME}-*" -print 2>/dev/null | sort)

print_section() {
    echo ""
    echo -e "${BLUE}━━━ $1 ━━━${NC}"
    echo ""
}

print_section "1. Checking directory structure"

echo "Current directory:"
pwd
echo ""

echo "Current directory content:"
ls -la
echo ""

echo "Checking if drivers/aic8800/ exists:"
if [ -d "drivers/aic8800" ]; then
    echo -e "${GREEN}✓${NC} drivers/aic8800 directory exists"
    echo ""
    echo "drivers/aic8800/ content:"
    ls -la drivers/aic8800/
    echo ""
    
    echo "Checking Makefile:"
    if [ -f "drivers/aic8800/Makefile" ]; then
        echo -e "${GREEN}✓${NC} Makefile found"
        echo ""
        echo "First 20 lines of Makefile:"
        head -20 drivers/aic8800/Makefile
    else
        echo -e "${RED}✗${NC} Makefile NOT FOUND!"
        echo "This is probably the problem!"
    fi
else
    echo -e "${RED}✗${NC} drivers/aic8800 directory NOT FOUND!"
    echo "The script expects to find the source code in drivers/aic8800/"
fi

print_section "2. Checking DKMS directory"

echo "Detected package: ${DRV_NAME}/${DRV_VERSION}"
if [ "${#DKMS_SOURCE_DIRS[@]}" -gt 0 ]; then
    for source_dir in "${DKMS_SOURCE_DIRS[@]}"; do
        echo -e "${GREEN}✓${NC} $source_dir directory exists"
        echo ""
        echo "Content:"
        ls -la "$source_dir/"
        echo ""

        echo "Checking drivers/aic8800 inside DKMS:"
        if [ -d "$source_dir/drivers/aic8800" ]; then
            echo -e "${GREEN}✓${NC} drivers/aic8800 copied to DKMS"
            ls -la "$source_dir/drivers/aic8800/"
        else
            echo -e "${RED}✗${NC} drivers/aic8800 NOT copied correctly!"
        fi
        echo ""
    done
else
    echo -e "${RED}✗${NC} No /usr/src/${DRV_NAME}-* directory found"
fi

print_section "3. Checking DKMS build logs"

mapfile -t BUILD_LOGS < <(find "/var/lib/dkms/${DRV_NAME}" -type f -name make.log -print 2>/dev/null | sort)

if [ "${#BUILD_LOGS[@]}" -gt 0 ]; then
    for build_log in "${BUILD_LOGS[@]}"; do
        echo -e "${GREEN}✓${NC} Build log found: $build_log"
        echo ""
        echo "Last 50 lines of make.log:"
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        tail -50 "$build_log"
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        echo ""
    done
else
    echo -e "${RED}✗${NC} No make.log found under /var/lib/dkms/${DRV_NAME}/"
    echo "The module may not have reached the build step yet, or this DKMS version stores logs elsewhere."
fi

if [ -f "$INSTALL_LOG" ]; then
    echo ""
    echo -e "${GREEN}✓${NC} Installer log found: $INSTALL_LOG"
    echo "Last 50 lines of installer log:"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    tail -50 "$INSTALL_LOG"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
else
    echo -e "${YELLOW}!${NC} Installer log not found: $INSTALL_LOG"
fi

print_section "4. Checking dkms.conf"

if [ "${#DKMS_SOURCE_DIRS[@]}" -gt 0 ]; then
    for source_dir in "${DKMS_SOURCE_DIRS[@]}"; do
        if [ -f "$source_dir/dkms.conf" ]; then
            echo -e "${GREEN}✓${NC} dkms.conf found: $source_dir/dkms.conf"
            echo ""
            cat "$source_dir/dkms.conf"
            echo ""
        else
            echo -e "${RED}✗${NC} dkms.conf NOT FOUND in $source_dir"
        fi
    done
else
    echo -e "${RED}✗${NC} No DKMS source directory available to inspect"
fi

if [ -f "./dkms.conf" ]; then
    echo ""
    echo "dkms.conf in current directory:"
    cat ./dkms.conf
fi

print_section "5. Checking firmware"

if [ -d "/lib/firmware/aic8800D80" ]; then
    echo -e "${GREEN}✓${NC} Firmware installed"
    ls -la /lib/firmware/aic8800D80/
else
    echo -e "${RED}✗${NC} Firmware NOT installed"
fi

print_section "6. System information"

echo "Kernel: $(uname -r)"
echo "Architecture: $(uname -m)"
echo ""
echo "Compiler version:"
if command -v gcc >/dev/null 2>&1; then
    gcc --version | head -1
elif command -v clang >/dev/null 2>&1; then
    clang --version | head -1
else
    echo "No gcc or clang found"
fi
echo ""
echo "Installed kernel headers:"
ls -d "/lib/modules/$(uname -r)/build" 2>/dev/null && echo "✓ Headers found" || echo "✗ Headers NOT found"

print_section "7. DKMS status"

echo "DKMS modules registered:"
if command -v dkms >/dev/null 2>&1; then
    dkms status
else
    echo -e "${RED}✗${NC} dkms command not found"
fi
