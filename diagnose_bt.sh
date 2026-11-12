#!/bin/bash
# Diagnose Bluetooth on AIC8800 Wi-Fi/Bluetooth combo adapters.
#
# aic_load_fw initializes the device and uploads the AIC firmware. The
# standard in-kernel btusb driver must then bind to the Bluetooth HCI USB
# interface. The removed aic_btusb module must not be installed or loaded.

echo "=== AIC8800 Bluetooth diagnostics ==="
echo ""

echo "1. AIC USB devices:"
if command -v lsusb >/dev/null 2>&1; then
    lsusb | grep -iE 'a69c|368b|3625' || echo "   No known AIC USB ID found"
else
    echo "   lsusb is not installed"
fi
echo ""

echo "2. USB interfaces and bound drivers:"
bt_interface_found=false
btusb_bound=false
zlp_target_found=false
for dev in /sys/bus/usb/devices/*; do
    [ -f "$dev/idVendor" ] || continue
    [ -f "$dev/idProduct" ] || continue

    vid=$(cat "$dev/idVendor" 2>/dev/null)
    pid=$(cat "$dev/idProduct" 2>/dev/null)
    case "$vid" in
        a69c|368b|3625) ;;
        *) continue ;;
    esac

    echo "   Device: $vid:$pid"
    if [ "$vid" = "368b" ] && [ "$pid" = "8d81" ]; then
        zlp_target_found=true
    fi
    for intf in "$dev"/*:*; do
        [ -d "$intf" ] || continue
        class=$(cat "$intf/bInterfaceClass" 2>/dev/null)
        subclass=$(cat "$intf/bInterfaceSubClass" 2>/dev/null)
        protocol=$(cat "$intf/bInterfaceProtocol" 2>/dev/null)
        driver=$(basename "$(readlink "$intf/driver" 2>/dev/null)" 2>/dev/null)
        echo "     $(basename "$intf"): ${class:-??}/${subclass:-??}/${protocol:-??} -> ${driver:-unbound}"

        if [ "$driver" = "btusb" ]; then
            btusb_bound=true
            bt_interface_found=true
        elif [ "$class" = "e0" ] && [ "$subclass" = "01" ] && [ "$protocol" = "01" ]; then
            bt_interface_found=true
        fi
    done
done
echo ""

echo "3. Relevant kernel modules (combo expectation: aic_load_fw + btusb):"
lsmod | grep -E '^(aic_load_fw|aic8800_fdrv|aic_zlp_quirk|aic_btusb|btusb|bluetooth)[[:space:]]' || \
    echo "   No related module is currently loaded"
echo ""

echo "4. Bluetooth controllers:"
if command -v bluetoothctl >/dev/null 2>&1; then
    bluetoothctl list 2>/dev/null || echo "   No controller reported by bluetoothctl"
elif command -v hciconfig >/dev/null 2>&1; then
    hciconfig -a 2>/dev/null || echo "   No controller reported by hciconfig"
else
    echo "   BlueZ command-line tools are not installed"
fi
echo ""

echo "5. Firmware, btusb, ZLP quirk, and HCI log messages:"
dmesg 2>/dev/null | grep -iE 'fw_patch|fw_adid|aicbt|aic_zlp_quirk|bluetooth|btusb|hci' | tail -80 || true
echo ""

echo "6. Bluetooth rfkill state:"
if command -v rfkill >/dev/null 2>&1; then
    rfkill list bluetooth 2>/dev/null || echo "   No Bluetooth rfkill entry"
else
    echo "   rfkill is not installed"
fi
echo ""

echo "7. Obsolete aic_btusb installation/configuration:"
legacy_refs_found=false
mapfile -t legacy_modprobe_files < <(
    grep -RIlE '^[[:space:]]*(softdep|alias)[^#]*aic_btusb([[:space:]]|$)' \
        /etc/modprobe.d /run/modprobe.d /usr/local/lib/modprobe.d \
        /usr/lib/modprobe.d /lib/modprobe.d 2>/dev/null | \
        grep -vE '\.aic8800-backup$' || true
)
mapfile -t legacy_udev_files < <(
    grep -RIl 'aic_btusb/new_id' \
        /etc/udev/rules.d /run/udev/rules.d \
        /usr/lib/udev/rules.d /lib/udev/rules.d 2>/dev/null | \
        grep -vE '\.aic8800-backup$' || true
)

for legacy_file in "${legacy_modprobe_files[@]}"; do
    echo "   Obsolete modprobe directive: $legacy_file"
    legacy_refs_found=true
done
for legacy_file in "${legacy_udev_files[@]}"; do
    echo "   Obsolete udev binding rule: $legacy_file"
    legacy_refs_found=true
done
if lsmod | awk '{print $1}' | grep -qx 'aic_btusb'; then
    echo "   Obsolete aic_btusb module is loaded"
    legacy_refs_found=true
fi
if modinfo -n aic_btusb >/dev/null 2>&1; then
    echo "   Obsolete module file: $(modinfo -n aic_btusb 2>/dev/null)"
    legacy_refs_found=true
fi
if [ "$legacy_refs_found" = false ]; then
    echo "   No active aic_btusb module, directive, or binding rule found (expected)"
fi
echo ""

echo "8. Device-scoped Bluetooth ACL ZLP quirk:"
if [ "$zlp_target_found" = true ]; then
    if [ -d /sys/module/aic_zlp_quirk ]; then
        zlp_hook="unknown"
        zlp_injections="unavailable"
        [ -r /sys/module/aic_zlp_quirk/parameters/hook ] && \
            zlp_hook=$(cat /sys/module/aic_zlp_quirk/parameters/hook)
        [ -r /sys/module/aic_zlp_quirk/parameters/injections ] && \
            zlp_injections=$(cat /sys/module/aic_zlp_quirk/parameters/injections)
        echo "   Target 368b:8d81 present"
        echo "   aic_zlp_quirk loaded: yes"
        echo "   active hook: $zlp_hook"
        echo "   ZLP injections: $zlp_injections"
    else
        echo "   Target 368b:8d81 present, but aic_zlp_quirk is not loaded"
    fi
else
    echo "   Target 368b:8d81 not present; quirk is not required"
fi
echo ""

echo "=== Assessment ==="
if ! lsmod | awk '{print $1}' | grep -qx 'aic_load_fw'; then
    echo "- aic_load_fw is not loaded; run: sudo modprobe aic8800_fdrv"
fi
if [ "$bt_interface_found" = true ] && [ "$btusb_bound" = false ]; then
    echo "- A Bluetooth USB interface exists but is not bound to btusb."
    echo "  Check for a btusb blacklist, then try: sudo modprobe btusb"
fi
if dmesg 2>/dev/null | grep -iE 'hci[0-9]+:.*(command|opcode|tx).*timed out|hci[0-9]+: link tx timeout' | tail -1 | grep -q .; then
    echo "- HCI timeout detected. Confirm that aic_load_fw uploaded the firmware before btusb bound."
    echo "  Also check for obsolete aic_btusb configuration above."
fi
if [ "$bt_interface_found" = false ]; then
    echo "- No Bluetooth HCI USB interface was found. This is normal for a Wi-Fi-only adapter."
fi
if [ "$zlp_target_found" = true ] && [ ! -d /sys/module/aic_zlp_quirk ]; then
    echo "- Device 368b:8d81 requires the ACL ZLP quirk, but the module is not loaded."
    echo "  Try: sudo modprobe aic_zlp_quirk"
fi

echo "=== Diagnostics complete ==="
