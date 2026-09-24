# Building with OpenWrt backports

OpenWrt's target kernel and wireless stack can have different versions.
For example, [issue #94](https://github.com/shenmintao/aic8800d80/issues/94)
uses Linux **6.12.94** with **backports 6.18.26** on OpenWrt 25.12.5.
The Ubuntu build host's kernel version is not used to select these APIs.

## Package Makefile

Pass `CFG80211_VERSION=6.18.26` to the kernel make invocation for that
combination. Use the `PKG_VERSION` from your OpenWrt
`package/kernel/mac80211/Makefile`, in `major.minor.patch` format. Keep the
backports include paths and symbol versions from the same build:

```make
NOSTDINC_FLAGS := \
	$(KERNEL_NOSTDINC_FLAGS) \
	-I$(STAGING_DIR)/usr/include/mac80211-backport/uapi \
	-I$(STAGING_DIR)/usr/include/mac80211-backport \
	-I$(STAGING_DIR)/usr/include/mac80211/uapi \
	-I$(STAGING_DIR)/usr/include/mac80211 \
	-include backport/backport.h

define Build/Compile
	+$(KERNEL_MAKE) $(PKG_JOBS) \
		M="$(PKG_BUILD_DIR)/drivers/aic8800" \
		KBUILD_EXTRA_SYMBOLS="$(LINUX_DIR)/../symvers/mac80211.symvers" \
		NOSTDINC_FLAGS="$(NOSTDINC_FLAGS)" \
		CFG80211_VERSION=6.18.26 \
		modules
endef
```

The package must depend on `kmod-cfg80211` so its headers and symbol versions
are staged before this driver is built. Keep any additional dependencies,
firmware installation rules and module packaging rules your device needs.
Update the package's pinned source revision to one containing this fix, or
apply the fix as an OpenWrt package patch, then clean and rebuild the package:

```sh
make package/kernel/aic8800d80/clean
make package/kernel/aic8800d80/compile V=sc -j1
```

Changing `CFG80211_VERSION` alone does not update the driver source or the
wireless headers. The value must match the actual wireless stack; do not set
it to the host kernel version or redefine `LINUX_VERSION_CODE`.

## Compatibility scope

The override selects the wireless API changes since Linux 6.12, including
monitor-channel callbacks, radio/link arguments, radar notifications and
newer cfg80211 callbacks. Timer, USB, memory-management and module namespace
checks continue to use the target kernel version. Older target kernels with
newer backports may need additional wireless API adaptations.

Leave `CFG80211_VERSION` unset when building against the kernel's own wireless
stack, as on a regular Ubuntu/Debian installation. The driver then uses the
kernel version as before. Hardware revision still determines whether to use
`main` or `legacy-mcu1`; a wireless API mismatch does not change that choice.

The feeds/Kconfig duplicate definitions and recursive dependencies also
reported in issue #94 belong to the OpenWrt build configuration and require
separate investigation.

See the [build regression recipe](tests/openwrt-backports/README.md) for
the tested kernel/backports combinations and a script to repeat the build.
