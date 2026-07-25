#!/usr/bin/env bash
set -euo pipefail

PACKAGE_NAME="aic-zlp-quirk"
PACKAGE_VERSION="0.1"
MODULE_NAME="aic_zlp_quirk"
KERNEL_RELEASE="${KERNEL_RELEASE:-$(uname -r)}"
KERNEL_BUILD_DIR="/lib/modules/${KERNEL_RELEASE}/build"
SOURCE_DIR="/usr/src/${PACKAGE_NAME}-${PACKAGE_VERSION}"
MODULES_LOAD_FILE="/etc/modules-load.d/${PACKAGE_NAME}.conf"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"
QUIRK_SOURCE="${REPO_ROOT}/drivers/aic8800/aic_zlp_quirk/aic_zlp_quirk.c"

log() {
    printf '[aic-zlp-quirk-test] %s\n' "$*"
}

die() {
    printf '[aic-zlp-quirk-test] ERROR: %s\n' "$*" >&2
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

module_loaded() {
    [ -d "/sys/module/$1" ]
}

legacy_btusb_loaded() {
    module_loaded aic_btusb
}

patched_btusb_installed() {
    dkms status -m btusb-aic-zlp -v 0.1 2>/dev/null | grep -q .
}

copy_sources() {
    install -d "${SOURCE_DIR}"
    install -m 0644 "${QUIRK_SOURCE}" "${SOURCE_DIR}/"
    install -m 0644 "${SCRIPT_DIR}/Makefile" "${SOURCE_DIR}/"
    install -m 0644 "${SCRIPT_DIR}/dkms.conf" "${SOURCE_DIR}/"
}

rollback_install() {
    log "rolling back the ZLP companion module"
    rm -f -- "${MODULES_LOAD_FILE}"
    modprobe -r "${MODULE_NAME}" >/dev/null 2>&1 || true
    dkms remove -m "${PACKAGE_NAME}" -v "${PACKAGE_VERSION}" --all >/dev/null 2>&1 || true
    rm -rf -- "${SOURCE_DIR}"
    depmod -a "${KERNEL_RELEASE}" >/dev/null 2>&1 || true
    start_bluetooth
}

verify_standard_btusb() {
    if legacy_btusb_loaded; then
        die "obsolete aic_btusb is loaded; run 'sudo modprobe -r aic_btusb' and remove its old autoload configuration first"
    fi

    if patched_btusb_installed; then
        die "the patched btusb test package is still installed; remove it before testing the companion module"
    fi

    modprobe btusb || die "could not load the system btusb module"
}

install_module() {
    require_root
    require_command dkms
    require_command make
    require_command modprobe
    require_command depmod

    [ -r "${KERNEL_BUILD_DIR}/Makefile" ] || \
        die "kernel build tree not found: ${KERNEL_BUILD_DIR}"

    if command -v lsusb >/dev/null 2>&1 && ! lsusb -d 368b:8d81 >/dev/null 2>&1; then
        die "this test is restricted to USB device 368b:8d81, which is not connected"
    fi

    verify_standard_btusb

    if module_loaded "${MODULE_NAME}"; then
        log "unloading the earlier companion module"
        if ! modprobe -r "${MODULE_NAME}"; then
            die "could not unload the earlier ${MODULE_NAME} module"
        fi
    fi

    trap rollback_install ERR

    if dkms status -m "${PACKAGE_NAME}" -v "${PACKAGE_VERSION}" 2>/dev/null | grep -q .; then
        log "removing an earlier companion-module build"
        dkms remove -m "${PACKAGE_NAME}" -v "${PACKAGE_VERSION}" --all
    fi

    rm -rf -- "${SOURCE_DIR}"
    copy_sources

    log "building the ZLP companion module for ${KERNEL_RELEASE}"
    dkms add -m "${PACKAGE_NAME}" -v "${PACKAGE_VERSION}"
    dkms build -m "${PACKAGE_NAME}" -v "${PACKAGE_VERSION}" -k "${KERNEL_RELEASE}"
    dkms install -m "${PACKAGE_NAME}" -v "${PACKAGE_VERSION}" \
        -k "${KERNEL_RELEASE}" --force
    depmod -a "${KERNEL_RELEASE}"

    printf '%s\n' "${MODULE_NAME}" > "${MODULES_LOAD_FILE}"

    stop_bluetooth
    if ! modprobe "${MODULE_NAME}"; then
        trap - ERR
        rollback_install
        die "the companion module could not attach; the system btusb was left unchanged"
    fi
    start_bluetooth
    trap - ERR

    log "loaded module: $(modinfo -n "${MODULE_NAME}")"
    log "system btusb remains active; test AAC playback for at least one hour"
}

remove_module() {
    require_root
    require_command dkms
    require_command modprobe
    require_command depmod

    stop_bluetooth
    rm -f -- "${MODULES_LOAD_FILE}"
    if module_loaded "${MODULE_NAME}" && ! modprobe -r "${MODULE_NAME}"; then
        start_bluetooth
        die "could not unload ${MODULE_NAME}; no files were removed"
    fi
    if dkms status -m "${PACKAGE_NAME}" -v "${PACKAGE_VERSION}" 2>/dev/null | grep -q .; then
        if ! dkms remove -m "${PACKAGE_NAME}" -v "${PACKAGE_VERSION}" --all; then
            start_bluetooth
            die "DKMS removal failed; source files were left in place"
        fi
    fi
    rm -rf -- "${SOURCE_DIR}"
    depmod -a "${KERNEL_RELEASE}"
    start_bluetooth

    log "removed the companion module; the distribution btusb was never replaced"
}

show_aic_binding() {
    local device
    local driver

    for device in /sys/bus/usb/devices/*; do
        [ -r "${device}/idVendor" ] || continue
        [ -r "${device}/idProduct" ] || continue
        [ "$(cat "${device}/idVendor")" = "368b" ] || continue
        [ "$(cat "${device}/idProduct")" = "8d81" ] || continue

        driver="not bound"
        if [ -L "${device}:1.0/driver" ]; then
            driver="$(basename "$(readlink -f "${device}:1.0/driver")")"
        fi

        printf 'AIC BT driver: %s\n' "${driver}"
        return
    done

    printf 'AIC BT driver: device not found\n'
}

show_status() {
    local loaded="no"
    local injections="unavailable"
    local hook="unavailable"
    local integrated_dkms_status
    local standalone_dkms_status

    if module_loaded "${MODULE_NAME}"; then
        loaded="yes"
        if [ -r "/sys/module/${MODULE_NAME}/parameters/injections" ]; then
            injections="$(cat "/sys/module/${MODULE_NAME}/parameters/injections")"
        fi
        if [ -r "/sys/module/${MODULE_NAME}/parameters/hook" ]; then
            hook="$(cat "/sys/module/${MODULE_NAME}/parameters/hook")"
        fi
    fi

    standalone_dkms_status="$(dkms status -m "${PACKAGE_NAME}" -v "${PACKAGE_VERSION}" 2>/dev/null || true)"
    if [ -z "${standalone_dkms_status}" ]; then
        standalone_dkms_status="not installed"
    fi
    integrated_dkms_status="$(dkms status -m aic8800 -v 1.0.0 2>/dev/null || true)"
    if [ -z "${integrated_dkms_status}" ]; then
        integrated_dkms_status="not installed"
    fi

    printf 'kernel:        %s\n' "${KERNEL_RELEASE}"
    printf 'btusb module:  %s\n' "$(modinfo -n btusb 2>/dev/null || printf 'not found')"
    printf 'quirk module:  %s\n' "$(modinfo -n "${MODULE_NAME}" 2>/dev/null || printf 'not found')"
    printf 'quirk loaded:  %s\n' "${loaded}"
    printf 'active hook:   %s\n' "${hook}"
    printf 'ZLP injections:%s\n' " ${injections}"
    printf 'legacy aic_btusb: %s\n' "$(legacy_btusb_loaded && printf 'loaded' || printf 'not loaded')"
    printf 'integrated DKMS: %s\n' "${integrated_dkms_status}"
    printf 'standalone DKMS: %s\n' "${standalone_dkms_status}"
    show_aic_binding
}

case "${1:-}" in
    install)
        install_module
        ;;
    remove)
        remove_module
        ;;
    status)
        show_status
        ;;
    *)
        printf 'Usage: %s {install|remove|status}\n' "$0" >&2
        exit 2
        ;;
esac
