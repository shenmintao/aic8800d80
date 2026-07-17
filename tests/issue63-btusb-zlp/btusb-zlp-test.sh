#!/usr/bin/env bash
set -euo pipefail

PACKAGE_NAME="btusb-aic-zlp"
PACKAGE_VERSION="0.1"
KERNEL_RELEASE="${KERNEL_RELEASE:-$(uname -r)}"
UPSTREAM_VERSION="${UPSTREAM_VERSION:-$(printf '%s' "$KERNEL_RELEASE" | sed -nE 's/^([0-9]+\.[0-9]+\.[0-9]+).*/\1/p')}"
UPSTREAM_TAG="${UPSTREAM_TAG:-v${UPSTREAM_VERSION}}"
KERNEL_BUILD_DIR="/lib/modules/${KERNEL_RELEASE}/build"
SOURCE_DIR="/usr/src/${PACKAGE_NAME}-${PACKAGE_VERSION}"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PATCH_FILE="${SCRIPT_DIR}/0001-Bluetooth-btusb-add-AIC-bulk-TX-ZLP-quirk.patch"
BASE_URL="https://raw.githubusercontent.com/gregkh/linux/${UPSTREAM_TAG}/drivers/bluetooth"

log() {
    printf '[btusb-zlp-test] %s\n' "$*"
}

die() {
    printf '[btusb-zlp-test] ERROR: %s\n' "$*" >&2
    exit 1
}

require_root() {
    [ "$(id -u)" -eq 0 ] || die "run this command as root"
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"
}

stop_bluetooth() {
    if command -v systemctl >/dev/null 2>&1; then
        systemctl stop bluetooth.service 2>/dev/null || true
    fi
}

start_bluetooth() {
    if command -v systemctl >/dev/null 2>&1; then
        systemctl start bluetooth.service 2>/dev/null || true
    fi
}

rollback_test_install() {
    log "rolling back to the distribution btusb module"
    dkms remove -m "${PACKAGE_NAME}" -v "${PACKAGE_VERSION}" --all >/dev/null 2>&1 || true
    rm -rf -- "${SOURCE_DIR}"
    depmod -a "${KERNEL_RELEASE}" || true
    modprobe btusb || true
    start_bluetooth
}

activate_test_module() {
    stop_bluetooth

    if ! modprobe -r btusb; then
        rollback_test_install
        die "could not unload btusb; disconnect active Bluetooth devices and retry"
    fi

    if ! modprobe btusb; then
        rollback_test_install
        die "the patched btusb could not load; the distribution module was restored"
    fi

    start_bluetooth
}

write_build_files() {
    cat > "${SOURCE_DIR}/Makefile" <<'EOF'
KVER ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVER)/build

obj-m += btusb.o
btusb-y := drivers/bluetooth/btusb.o
ccflags-y += -I$(src)/drivers/bluetooth

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
EOF

    cat > "${SOURCE_DIR}/dkms.conf" <<'EOF'
PACKAGE_NAME="btusb-aic-zlp"
PACKAGE_VERSION="0.1"
MAKE[0]="make KVER=${kernelver}"
CLEAN="make KVER=${kernelver} clean"
BUILT_MODULE_NAME[0]="btusb"
BUILT_MODULE_LOCATION[0]="."
DEST_MODULE_LOCATION[0]="/updates/dkms"
AUTOINSTALL="no"
EOF
}

download_sources() {
    local file

    install -d "${SOURCE_DIR}/drivers/bluetooth"

    for file in btusb.c btintel.h btbcm.h btrtl.h btmtk.h; do
        log "downloading ${file} from Linux ${UPSTREAM_TAG}"
        curl --fail --location --silent --show-error \
            "${BASE_URL}/${file}" \
            --output "${SOURCE_DIR}/drivers/bluetooth/${file}"
    done
}

install_test_module() {
    require_root
    require_command curl
    require_command dkms
    require_command make
    require_command modprobe
    require_command patch

    [ -n "${UPSTREAM_VERSION}" ] || die "could not derive an upstream version from ${KERNEL_RELEASE}"
    [ -r "${KERNEL_BUILD_DIR}/Makefile" ] || die "kernel build tree not found: ${KERNEL_BUILD_DIR}"
    [ -r "${PATCH_FILE}" ] || die "patch file not found: ${PATCH_FILE}"

    if command -v lsusb >/dev/null 2>&1 && ! lsusb -d 368b:8d81 >/dev/null 2>&1; then
        die "the test patch is restricted to USB device 368b:8d81, which is not connected"
    fi

    if dkms status -m "${PACKAGE_NAME}" -v "${PACKAGE_VERSION}" 2>/dev/null | grep -q .; then
        log "removing an earlier test build"
        dkms remove -m "${PACKAGE_NAME}" -v "${PACKAGE_VERSION}" --all || true
    fi

    rm -rf -- "${SOURCE_DIR}"
    download_sources
    write_build_files

    log "applying the AIC bulk TX ZLP quirk"
    patch --directory "${SOURCE_DIR}" --strip 1 --input "${PATCH_FILE}"

    log "building patched system btusb for ${KERNEL_RELEASE}"
    dkms add -m "${PACKAGE_NAME}" -v "${PACKAGE_VERSION}"
    dkms build -m "${PACKAGE_NAME}" -v "${PACKAGE_VERSION}" -k "${KERNEL_RELEASE}"
    dkms install -m "${PACKAGE_NAME}" -v "${PACKAGE_VERSION}" \
        -k "${KERNEL_RELEASE}" --force
    depmod -a "${KERNEL_RELEASE}"

    activate_test_module

    log "loaded module: $(modinfo -n btusb)"
    log "test AAC playback for at least one hour; use '$0 remove' to restore the distribution module"
}

remove_test_module() {
    require_root
    require_command dkms
    require_command modprobe

    stop_bluetooth
    modprobe -r btusb || {
        start_bluetooth
        die "could not unload btusb; disconnect active Bluetooth devices and retry"
    }

    dkms remove -m "${PACKAGE_NAME}" -v "${PACKAGE_VERSION}" --all || true
    rm -rf -- "${SOURCE_DIR}"
    depmod -a "${KERNEL_RELEASE}"

    modprobe btusb || {
        start_bluetooth
        die "the distribution btusb module could not be loaded"
    }
    start_bluetooth

    log "restored module: $(modinfo -n btusb)"
}

show_status() {
    printf 'kernel:        %s\n' "${KERNEL_RELEASE}"
    printf 'upstream tag:  %s\n' "${UPSTREAM_TAG}"
    printf 'btusb module:  %s\n' "$(modinfo -n btusb 2>/dev/null || printf 'not found')"
    printf 'dkms status:   %s\n' "$(dkms status -m "${PACKAGE_NAME}" -v "${PACKAGE_VERSION}" 2>/dev/null || printf 'not installed')"
}

case "${1:-}" in
    install)
        install_test_module
        ;;
    remove)
        remove_test_module
        ;;
    status)
        show_status
        ;;
    *)
        printf 'Usage: %s {install|remove|status}\n' "$0" >&2
        exit 2
        ;;
esac
