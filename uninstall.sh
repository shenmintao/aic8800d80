#!/bin/bash
#
# Remove what install.sh installed: the DKMS package and its source tree,
# the AIC8800 firmware directories, the udev rules, the usb_modeswitch
# configuration and the installer log.
#
# Usage: sudo ./uninstall.sh [--dry-run]
#
# --dry-run prints every action without changing anything (no root needed).
# Firmware is moved to /var/backups instead of being deleted, and firmware
# owned by a distribution package is never touched.

set -euo pipefail

readonly DRV_NAME="aic8800"
# DESTDIR (default empty) prefixes every path this script removes. It exists
# for the test suite; dkms, modprobe and initramfs tools still act on the
# live system and are stubbed by the tests.
DESTDIR="${DESTDIR:-}"
readonly DESTDIR
readonly FW_ROOT="${DESTDIR}/lib/firmware"
readonly LOG_FILE="${DESTDIR}/var/log/aic8800-install.log"
readonly MODESWITCH_CONF="${DESTDIR}/etc/usb_modeswitch.d/1111:1111"
readonly -a RULES_FILES=(
    "${DESTDIR}/etc/udev/rules.d/aic.rules"
    "${DESTDIR}/usr/lib/udev/rules.d/aic.rules"
    "${DESTDIR}/lib/udev/rules.d/aic.rules"
)

DRY_RUN=0
case "${1:-}" in
    "") ;;
    --dry-run) DRY_RUN=1 ;;
    -h | --help)
        sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'
        exit 0
        ;;
    *)
        echo "Unknown option: $1 (use --dry-run or --help)" >&2
        exit 2
        ;;
esac

if [ "$DRY_RUN" = 0 ] && [ -z "$DESTDIR" ] && [ "$EUID" -ne 0 ]; then
    echo "This script must be run as root (sudo ./uninstall.sh), or with --dry-run." >&2
    exit 1
fi

# Print a command, then run it unless this is a dry run.
run() {
    echo "+ $*"
    if [ "$DRY_RUN" = 0 ]; then
        "$@"
    fi
}

# Print the package (dpkg, rpm or pacman) that owns a path, if any.
firmware_path_owner() {
    local path="$1" alt owner=""
    alt="/usr${path}"
    [[ "$path" == /usr/* ]] && alt="${path#/usr}"

    if command -v dpkg > /dev/null 2>&1; then
        owner=$(dpkg -S "$path" 2> /dev/null || dpkg -S "$alt" 2> /dev/null || true)
        owner="${owner%%:*}"
    fi
    if [ -z "$owner" ] && command -v rpm > /dev/null 2>&1; then
        owner=$(rpm -qf "$path" 2> /dev/null || true)
        [[ "$owner" == *"not owned"* ]] && owner=""
    fi
    if [ -z "$owner" ] && command -v pacman > /dev/null 2>&1; then
        owner=$(pacman -Qqo "$path" 2> /dev/null || true)
    fi
    printf '%s' "$owner"
}

[ "$DRY_RUN" = 1 ] && echo "Dry run: nothing will be changed."

echo "==> Unloading modules"
for module in aic8800_fdrv aic_zlp_quirk aic_load_fw; do
    if [ -d "/sys/module/$module" ]; then
        run modprobe -r "$module" || echo "    could not unload $module; it will be gone after a reboot"
    fi
done

# A DKMS registration that could not be removed still needs its source tree
# to rebuild or be removed later, so its tree is kept and the script fails.
exit_status=0
keep_all_sources=0
declare -A keep_source=()

echo "==> Removing DKMS registrations"
if command -v dkms > /dev/null 2>&1; then
    versions=()
    if dkms_status=$(dkms status 2> /dev/null); then
        # "dkms status" prints "aic8800/1.0.0, <kernel>, <arch>: installed".
        mapfile -t versions < <(printf '%s\n' "$dkms_status" | sed -nE "s#^${DRV_NAME}/([^,:]+)[,:].*#\1#p" | sort -u)
        [ "${#versions[@]}" -eq 0 ] && echo "    none found"
    else
        echo "    dkms status failed; keeping every ${DRV_NAME} source tree"
        keep_all_sources=1
        exit_status=1
    fi
    for version in "${versions[@]}"; do
        if ! run dkms remove "${DRV_NAME}/${version}" --all; then
            echo "    dkms remove ${DRV_NAME}/${version} failed; keeping its source tree"
            keep_source["$version"]=1
            exit_status=1
        fi
    done
else
    echo "    dkms is not installed; skipping"
fi

echo "==> Removing DKMS source trees"
found=0
for src in "${DESTDIR}/usr/src/${DRV_NAME}"-*; do
    [ -d "$src" ] || continue
    # Only remove trees that are this driver, identified by their dkms.conf.
    if grep -q "^PACKAGE_NAME=\"${DRV_NAME}\"" "$src/dkms.conf" 2> /dev/null; then
        found=1
        # DKMS names source trees /usr/src/<name>-<version>.
        version="${src##*/"${DRV_NAME}"-}"
        if [ "$keep_all_sources" = 1 ]; then
            echo "    keeping $src (DKMS registrations could not be listed)"
        elif [ -n "${keep_source[$version]:-}" ]; then
            echo "    keeping $src (its DKMS registration is still present)"
        else
            run rm -rf "$src"
        fi
    else
        echo "    leaving $src (no matching dkms.conf)"
    fi
done
[ "$found" = 0 ] && echo "    none found"

echo "==> Moving firmware to a backup directory"
mapfile -t fw_dirs < <(find "$FW_ROOT" -maxdepth 1 -name 'aic8800*' -type d 2> /dev/null | sort)
to_move=()
for dir in "${fw_dirs[@]}"; do
    owner=$(firmware_path_owner "${dir#"$DESTDIR"}")
    if [ -n "$owner" ]; then
        echo "    leaving $dir (owned by package '$owner')"
    else
        to_move+=("$dir")
    fi
done
if [ "${#to_move[@]}" -gt 0 ]; then
    backup_dir="${DESTDIR}/var/backups/aic8800-firmware-$(date '+%Y%m%d-%H%M%S')"
    run mkdir -p "$backup_dir"
    run mv "${to_move[@]}" "$backup_dir/"
    echo "    firmware saved in $backup_dir; delete it when no longer needed"
else
    echo "    none found"
fi

echo "==> Removing udev rules and usb_modeswitch configuration"
declare -A seen_rules=()
for rules in "${RULES_FILES[@]}"; do
    # /lib and /usr/lib are the same directory on merged-/usr systems.
    real=$(readlink -f "$rules" 2> /dev/null || echo "$rules")
    [ -n "${seen_rules[$real]:-}" ] && continue
    seen_rules[$real]=1
    # Only remove files that are this project's rules and that no package owns.
    if [ -f "$rules" ] && grep -q 'AIC8800' "$rules" 2> /dev/null; then
        owner=$(firmware_path_owner "${rules#"$DESTDIR"}")
        if [ -n "$owner" ]; then
            echo "    leaving $rules (owned by package '$owner')"
        else
            run rm -f "$rules"
        fi
    fi
done
if [ -f "$MODESWITCH_CONF" ] && grep -q 'AIC8800' "$MODESWITCH_CONF" 2> /dev/null; then
    owner=$(firmware_path_owner "${MODESWITCH_CONF#"$DESTDIR"}")
    if [ -n "$owner" ]; then
        echo "    leaving $MODESWITCH_CONF (owned by package '$owner')"
    else
        run rm -f "$MODESWITCH_CONF"
    fi
fi
if command -v udevadm > /dev/null 2>&1; then
    run udevadm control --reload-rules || true
fi

echo "==> Refreshing initramfs"
if command -v update-initramfs > /dev/null 2>&1; then
    run update-initramfs -u -k "$(uname -r)" || echo "    update-initramfs failed"
elif command -v dracut > /dev/null 2>&1; then
    run dracut -f || echo "    dracut failed"
elif command -v mkinitcpio > /dev/null 2>&1; then
    run mkinitcpio -P || echo "    mkinitcpio failed"
else
    echo "    no supported initramfs tool found; skipping"
fi

if [ -f "$LOG_FILE" ]; then
    run rm -f "$LOG_FILE"
fi

if [ "$exit_status" -ne 0 ]; then
    echo ""
    echo "Some DKMS registrations could not be removed, so their source trees were kept." >&2
    echo "Check 'dkms status', remove them with 'sudo dkms remove ${DRV_NAME}/<version> --all'," >&2
    echo "then run this script again." >&2
fi

echo ""
echo "Done. Not removed: packages installed as dependencies (dkms, headers,"
echo "usb_modeswitch, ...), firmware backups in /var/backups/aic8800-firmware-*,"
echo "and *.aic8800-backup files that install.sh made of legacy configuration."

exit "$exit_status"
