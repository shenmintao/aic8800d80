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

if [ -d "/usr/src/aic8800-1.0.0" ]; then
    echo -e "${GREEN}✓${NC} /usr/src/aic8800-1.0.0 directory exists"
    echo ""
    echo "Content:"
    ls -la /usr/src/aic8800-1.0.0/
    echo ""
    
    echo "Checking drivers/aic8800 inside DKMS:"
    if [ -d "/usr/src/aic8800-1.0.0/drivers/aic8800" ]; then
        echo -e "${GREEN}✓${NC} drivers/aic8800 copied to DKMS"
        ls -la /usr/src/aic8800-1.0.0/drivers/aic8800/
    else
        echo -e "${RED}✗${NC} drivers/aic8800 NOT copied correctly!"
    fi
else
    echo -e "${RED}✗${NC} /usr/src/aic8800-1.0.0 directory NOT FOUND"
fi

print_section "3. Checking DKMS build logs"

if [ -f "/var/lib/dkms/aic8800/1.0.0/build/make.log" ]; then
    echo -e "${GREEN}✓${NC} Build log found"
    echo ""
    echo "Last 50 lines of make.log:"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    tail -50 /var/lib/dkms/aic8800/1.0.0/build/make.log
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
else
    echo -e "${RED}✗${NC} Build log not found in /var/lib/dkms/aic8800/1.0.0/build/make.log"
fi

print_section "4. Checking dkms.conf"

if [ -f "/usr/src/aic8800-1.0.0/dkms.conf" ]; then
    echo -e "${GREEN}✓${NC} dkms.conf found"
    echo ""
    echo "dkms.conf content:"
    cat /usr/src/aic8800-1.0.0/dkms.conf
else
    echo -e "${RED}✗${NC} dkms.conf NOT FOUND!"
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
echo "GCC version:"
gcc --version | head -1
echo ""
echo "Installed kernel headers:"
ls -d /lib/modules/$(uname -r)/build 2>/dev/null && echo "✓ Headers found" || echo "✗ Headers NOT found"

print_section "7. DKMS status"

echo "DKMS modules registered:"
dkms status
