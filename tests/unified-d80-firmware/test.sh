#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
fw_root="$repo_root/fw/aic8800D80"
loader="$repo_root/drivers/aic8800/aic_load_fw/aicbluetooth.c"
profile="$repo_root/drivers/aic8800/aic_load_fw/aic_compat_8800d80.c"
zlp_quirk="$repo_root/drivers/aic8800/aic_zlp_quirk/aic_zlp_quirk.c"
driver_makefile="$repo_root/drivers/aic8800/Makefile"

required_files='aic_userconfig_8800d80.txt
fmacfw_8800d80_u02.bin
fw_adid_8800d80_u02.bin
fw_patch_8800d80_u02.bin
fw_patch_table_8800d80_u02.bin'

for profile_dir in mcu0 mcu1; do
    echo "$required_files" | while IFS= read -r file; do
        test -s "$fw_root/$profile_dir/$file" || {
            echo "missing firmware: $profile_dir/$file" >&2
            exit 1
        }
    done
done

for file in fmacfw_8800d80_u02.bin fw_patch_8800d80_u02.bin; do
    cmp -s "$fw_root/mcu0/$file" "$fw_root/mcu1/$file" && {
        echo "MCU profiles unexpectedly contain identical $file" >&2
        exit 1
    }
done

grep -q 'chip_mcu_id ? "aic8800D80/mcu1"' "$loader"
grep -q 'patch_tbl_d80_mcu1' "$profile"
grep -q 'Using MCU0 current patch profile' "$profile"
grep -q 'CONFIG_USE_FW_REQUEST = y' "$driver_makefile"
grep -q 'USB_DEVICE(0x368b, 0x8d81)' "$zlp_quirk"
if grep -q 'USB_DEVICE(0xa69c, 0x8d81)' "$zlp_quirk"; then
    echo "a69c:8d81 must not use the incompatible Bluetooth ZLP quirk" >&2
    exit 1
fi

echo "unified D80 firmware layout: OK"
