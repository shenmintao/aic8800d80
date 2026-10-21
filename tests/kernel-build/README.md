# Native kernel build regression

Build the current driver against real, prepared kernel headers and symbol
versions. The script cleans a temporary source copy, builds all three modules
and leaves the results for inspection. It does not install or load them.

```sh
bash tests/kernel-build/build.sh /path/to/kernel/build CC=gcc-14
```

Set `JOBS` to choose parallelism and `TMPDIR` to an existing directory if
build outputs need to survive system temporary-directory cleanup.

Use the kernel's matching compiler and configuration. For a custom kernel,
pass the exact output directory used to build it, including `O=...` when
applicable. `make modules_prepare` alone does not generate `Module.symvers`;
see the [external module build documentation](https://docs.kernel.org/kbuild/modules.html).

For OpenWrt wireless backports, use the separate
[backports build check](../openwrt-backports/README.md), which supplies the
staged wireless headers, symbol versions and `CFG80211_VERSION` together.

## Issue #111: Linux 7.3 on legacy-mcu1

The `legacy-mcu1` branch was missing `main`'s Linux 7.3 compatibility commit
[`2c4ab4b`](https://github.com/shenmintao/aic8800d80/commit/2c4ab4b421d6a780842deeef700f23ba9ecf7578).
The backport adapts `cfg80211_probe_status()`'s link ID, the renamed
`probe_peer` callback, and the input cookies for probe, management TX and
remain-on-channel operations. Completion notifications retain the cookie
assigned by cfg80211; internal TX/RoC callers keep their driver-assigned
cookies. The existing MCU1 firmware and loader profile are unchanged.

The original branch at `e074b7e` reproduces all six errors from the
[issue's build log](https://github.com/shenmintao/aic8800d80/issues/111#issuecomment-6294321166)
against Linux 7.3-rc3 headers: two probe-status argument errors, the renamed
probe callback and its resulting initializer error, and incompatible
management TX and remain-on-channel callback types.

## Verified on 2026-10-21

| Kernel/build headers | Fixed legacy-mcu1 result |
| --- | --- |
| Upstream Linux 7.3.0 final, x86_64 test configuration | All three modules built; MODPOST passed |
| Ubuntu 7.3.0-070300rc3-generic headers | All three modules built; MODPOST passed |
| Ubuntu 7.2.0-070200-generic headers | All three modules built; MODPOST passed |
| Ubuntu 6.17.0-14-generic headers | All three modules built; MODPOST passed |

The final-release check uses the upstream `linux-7.3.tar.xz` source, checked
against kernel.org's SHA-256 manifest. The test kernel enables modules,
symbol versioning, cfg80211, USB, Bluetooth and kprobes. Both `vmlinux` and
the in-tree modules were built with GCC 14.2.0 to generate matching headers
and `Module.symvers` before running this script. No symbol-version or
MODPOST checks were bypassed. All three modules report `7.3.0` vermagic and
the same `module_layout` CRC as that kernel's `Module.symvers`.

The Ubuntu header checks also use GCC 14.2.0. The 7.2 and 7.3-rc3 headers
were generated with GCC 15 and produce compiler/attribute warnings; BTF
generation is skipped where a matching `vmlinux` is unavailable. The rc3
`cfg80211.h` is byte-identical to the
[upstream 7.3 final header](https://github.com/torvalds/linux/blob/v7.3/include/net/cfg80211.h).

These checks cover compilation and symbol resolution. They do not test the
reporter's custom `7.3.0-lxpup64` configuration, module loading, firmware
startup, Wi-Fi traffic or Bluetooth operation on hardware.

After a successful build, install only the newly built modules against the
same kernel. Chain build and installation with `&&` so a failed build cannot
lead to an attempt to install stale modules. MCU1 hardware should continue
using `legacy-mcu1`.
