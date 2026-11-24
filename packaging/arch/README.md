# Arch package for the AIC8800D80 driver

`PKGBUILD` builds `aic8800d80-dkms-git`: the out-of-tree driver as a DKMS
module, every firmware variant, the udev rule and the `usb_modeswitch` data
the rule needs.

## Build and install

```sh
cd packaging/arch
makepkg -f
sudo pacman -U aic8800d80-dkms-git-*.pkg.tar.zst
```

The package writes only to `/usr` (`/usr/src/aic8800-<pkgver>`,
`/usr/lib/firmware/aic8800*`, `/usr/share/usb_modeswitch/1111:1111`,
`/usr/lib/udev/rules.d/99-aic8800.rules`); pacman owns every file it installs.

## Migrating from install.sh

`install.sh` manages the driver outside the package manager. On a machine that
used it, these paths exist and no package owns them:

| Path | Left by |
| --- | --- |
| `/etc/udev/rules.d/aic.rules` | `install.sh` (udev rule) |
| `/etc/usb_modeswitch.d/1111:1111` | `install.sh` (mode-switch data) |
| `/lib/firmware/aic8800*` | `install.sh` (firmware directories) |
| `/usr/src/aic8800-1.0.0` | `install.sh` (DKMS source tree) |
| DKMS registration `aic8800/1.0.0` | `dkms add` by `install.sh` |

`/etc/udev/rules.d/aic.rules` and the packaged
`/usr/lib/udev/rules.d/99-aic8800.rules` have different file names, so udev
would load both and run both mode-switch commands for the same `1111:1111`
`add` event. The install scriptlet (`aic8800d80-dkms-git.install`) therefore
runs before pacman extracts any file:

- `pre_install` and `pre_upgrade` move every unowned path above into
  `/var/backups/aic8800d80-migration-<timestamp>/`, keeping its absolute path
  below the backup directory. Nothing is deleted.
- `dkms remove aic8800/1.0.0 --all` is called when that registration exists
  and its source tree is unowned. If the removal fails, the failure is
  reported and the source tree is kept, so DKMS is never left with a
  registration that points at a missing tree.
- A path owned by any package (`pacman -Qqo`) is never moved or removed. If a
  package owns `/etc/udev/rules.d/aic.rules`, the scriptlet says so: that rule
  keeps running next to `99-aic8800.rules`.
- `post_install` and `post_upgrade` run `udevadm control --reload-rules` and
  replay the `add` event only for a `1111:1111` adapter, not for every USB
  device.
- The migration is idempotent: a second run finds nothing to move and creates
  no second backup directory.

Restore a file by copying it back from the backup directory, e.g.

```sh
sudo cp -a /var/backups/aic8800d80-migration-*/etc/udev/rules.d/aic.rules \
  /etc/udev/rules.d/aic.rules
```

After the switch, `dkms status` should list only `aic8800/<pkgver>`, and
`/etc/udev/rules.d/aic.rules` should be gone. Reboot (or reload the modules)
once the DKMS build for the running kernel has finished, because the module
that `install.sh` built may still be loaded.

## Tests

```sh
cd packaging/arch
./tests/migration-from-install-sh.sh
# also check the files a built package really contains:
./tests/migration-from-install-sh.sh --package aic8800d80-dkms-git-*.pkg.tar.zst
# same scenario with the real dkms in a bwrap sandbox (needs bwrap and dkms):
./tests/migration-e2e-bwrap.sh --package aic8800d80-dkms-git-*.pkg.tar.zst
```

`migration-from-install-sh.sh` builds a temporary root that looks like a
script-managed installation, puts stub `pacman`, `dkms` and `udevadm` in
`PATH`, sources the install scriptlet and calls `pre_install`/`post_install`
with `AIC8800_ROOT` pointing at that root. It checks that the old udev rule,
mode-switch data, firmware and DKMS source tree end up in the backup
byte-identical, that `dkms remove aic8800/1.0.0 --all` is called, that
package-owned paths are untouched, that only `99-aic8800.rules` remains after
the package is unpacked, and that the second run is a no-op. Nothing outside
the temporary directory is written, so the test can run next to an installed
driver.

`migration-e2e-bwrap.sh` repeats the scenario with the real `dkms` and the real
merged-`/usr` layout (`/lib` -> `/usr/lib`) inside a bwrap sandbox whose
`/etc`, `/var`, `/usr/src` and `/usr/lib/firmware` are private copies; it
exits 77 when bwrap or dkms is missing.

Both are installed as
`/usr/share/aic8800d80-dkms-git/tests/migration-*.sh`; there they take the
scriptlet from the pacman database copy (`/var/lib/pacman/local/<pkg>/install`)
and the driver tree from `/usr/src/aic8800-<pkgver>`.
