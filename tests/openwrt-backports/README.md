# OpenWrt backports build regression

This checks complete module compilation and MODPOST against real kernel and
wireless-stack headers. It requires an already prepared SDK/buildroot with
cfg80211/mac80211 headers and `mac80211.symvers` staged. It does not install
packages or load modules.

For the x86_64 SDK used in issue #94:

```sh
sdk=/path/to/openwrt-sdk
kernel="$sdk/build_dir/target-x86_64_musl/linux-x86_64/linux-6.12.94"
export STAGING_DIR="$sdk/staging_dir/target-x86_64_musl"
export PATH="$sdk/staging_dir/toolchain-x86_64_gcc-14.3.0_musl/bin:$PATH"

bash tests/openwrt-backports/build.sh \
    "$kernel" "$STAGING_DIR/usr/include" \
    "$kernel/../symvers/mac80211.symvers" 6.18.26 \
    ARCH=x86_64 CROSS_COMPILE=x86_64-openwrt-linux-musl-
```

The script copies the current driver into a fresh temporary directory and
checks that all three `.ko` files are produced. Additional make arguments
can select a compiler or architecture. Use matching headers, configuration
and symbol versions throughout; changing only the version argument is not
a substitute for the matching wireless stack.

## Verified on 2026-09-17

- OpenWrt 25.12.5 x86_64 SDK: Linux 6.12.94, GCC 14.3.0, backports 6.18.26
  with the mac80211 patches from OpenWrt revision `f5dae5ece4` applied:
  all three modules compile and pass MODPOST with the kernel's `-Werror`.
- Original driver revision `9594c5c`, on the same kernel/backports combination:
  reproduces both RX argument-count errors and the four incompatible
  `set_monitor_channel`, `set_wiphy_params`, `set_tx_power`, `get_tx_power`
  callbacks. This comparison uses `-Wno-error=unused-label`, as the issue's
  log does, to reach those failures. The fixed build needs no such override.
- Native kernel headers 6.11.0-17, 6.17.0-14 and 7.2.0-070200, with
  `CFG80211_VERSION` unset: all three modules compile and pass MODPOST.
  The 7.2 check uses GCC 14 and reports compiler/attribute warnings because
  those headers were built with GCC 15.

These are build checks. They do not validate an installable OpenWrt package,
firmware loading, Wi-Fi connectivity or Bluetooth operation on hardware.
