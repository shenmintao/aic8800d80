# Firmware loading regression checks

Run `python3 tests/firmware/test_request_firmware.py` with a C compiler available.
The fixture extracts both modules' real request and load functions and the
Wi-Fi common caller. It checks all USB chip directory mappings, device context,
flat-layout fallback only on `-ENOENT`, missing device data, non-USB/platform
behavior, path overflow, invalid file sizes, and allocation failure. Kernel
services are mocked; this does not exercise firmware startup or hardware.

`python3 tests/firmware/test_firmware.py --modules BUILD_DIR` also checks the
installed inventory against the built modules' firmware declarations and the
loader's probe error paths.

For a kernel build with the firmware API enabled:

```sh
bash tests/kernel-build/build.sh /path/to/kernel/build CC=gcc-14 CONFIG_USE_FW_REQUEST=y
```

The build default remains `CONFIG_USE_FW_REQUEST=n`. The `y` path searches the
kernel firmware locations using chip-relative names such as
`aic8800D80/fmacfw_8800d80_u02.bin`, then tries the bare name if the chip-relative
file is absent. The two modules use different chip ID enums; their mappings
must remain separate. Both retain the real USB device as the request context.
Custom absolute `aic_fw_path` directories belong to the existing `n` path;
the `y` path uses the kernel firmware search configuration instead.

Related: [issue #117](https://github.com/shenmintao/aic8800d80/issues/117).
The path correction follows ggorbi99-dev's report, with the operational-driver
IDs and its previously null device argument corrected as well. It complements
the initramfs declarations; it does not establish the reporter's cold-boot fix.
