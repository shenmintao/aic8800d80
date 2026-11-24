# Arch package for the AIC8800D80 driver

`PKGBUILD` builds `aic8800d80-dkms-git`: the out-of-tree driver as a DKMS
module, every firmware variant, the udev rule and the `usb_modeswitch` data
the rule needs.

## Build and install

```sh
cd packaging/arch
makepkg -f
```

On a machine that never used `install.sh`:

```sh
sudo pacman -U aic8800d80-dkms-git-*.pkg.tar.zst
```

On a machine that used `install.sh`, migrate first:

```sh
sudo ./migrate-from-install-sh.sh          # add --dry-run to look first
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

Two things must go away for the package to work:

- the old `/etc/udev/rules.d/aic.rules` and the packaged
  `/usr/lib/udev/rules.d/99-aic8800.rules` have different file names, so udev
  would load both and run both mode-switch commands for the same `1111:1111`
  `add` event;
- the firmware in `/lib/firmware/aic8800*` is the same file set the package
  ships (`/lib` is `/usr/lib` on merged-`/usr` systems), so pacman sees the
  files as unowned and refuses the transaction.

**`pre_install` cannot fix the second one.** pacman compares file lists in
`_alpm_sync_check()` before it commits the transaction, and `pre_install` runs
inside the commit. A plain `pacman -U` on a script-managed machine therefore
stops with `exists in filesystem` and the scriptlet never runs. There are two
ways to switch:

### Recommended: migrate first

```sh
sudo ./migrate-from-install-sh.sh --dry-run   # shows what would move
sudo ./migrate-from-install-sh.sh             # asks for confirmation
sudo pacman -U aic8800d80-dkms-git-*.pkg.tar.zst
```

The script runs from a checkout or from the copy installed at
`/usr/share/aic8800d80-dkms-git/migrate-from-install-sh.sh`. It moves every
unowned path from the table above into
`/var/backups/aic8800d80-migration-<timestamp>/` (keeping its absolute path),
runs `dkms remove aic8800/1.0.0 --all` when that registration exists and its
source tree is unowned, and then re-checks that nothing is left; it exits
non-zero if a file could not be moved. Nothing is deleted, and a path owned by
any package (`pacman -Qqo`) is never touched. If `/etc/udev/rules.d/aic.rules`
belongs to another package, the script says so: that rule will keep running
next to `99-aic8800.rules`.

### Alternative: let `--overwrite` reach the scriptlet

```sh
sudo pacman -U --overwrite '/usr/lib/firmware/aic8800*' aic8800d80-dkms-git-*.pkg.tar.zst
```

`--overwrite` skips the file conflict for those paths, so pacman commits the
transaction and `pre_install` moves the old firmware and the udev rule before
extraction. This works, but it has to be typed correctly, it only covers the
firmware paths, and a later plain `pacman -U` of a newer build would hit the
conflict again while the leftover firmware exists.

### What the install scriptlet still does

`aic8800d80-dkms-git.install` keeps the same migration in
`pre_install`/`pre_upgrade` as a safety net for everything that does not
conflict: the `/etc` udev rule, `/etc/usb_modeswitch.d/1111:1111`,
`/usr/src/aic8800-1.0.0` and the `aic8800/1.0.0` registration (the firmware too
when the transaction is reached with `--overwrite`). `post_install` and
`post_upgrade` run `udevadm control --reload-rules` and replay the `add` event
only for a `1111:1111` adapter. The migration is idempotent: a second run finds
nothing to move and creates no second backup directory.

### Restoring and checking

Restore a file by copying it back from the backup directory, e.g.

```sh
sudo cp -a /var/backups/aic8800d80-migration-*/etc/udev/rules.d/aic.rules \
  /etc/udev/rules.d/aic.rules
```

After the switch, `dkms status` should list only `aic8800/<pkgver>`,
`/etc/udev/rules.d/aic.rules` should be gone, and
`grep -rl 'usb_modeswitch -b' /etc/udev/rules.d /usr/lib/udev/rules.d` should
only return `99-aic8800.rules`. Reboot (or reload the modules) once the DKMS
build for the running kernel has finished, because the module that `install.sh`
built may still be loaded.

## Tests

```sh
cd packaging/arch
./tests/migration-from-install-sh.sh
# also check the files a built package really contains:
./tests/migration-from-install-sh.sh --package aic8800d80-dkms-git-*.pkg.tar.zst
# same scenario with the real dkms in a bwrap sandbox (needs bwrap and dkms):
./tests/migration-e2e-bwrap.sh --package aic8800d80-dkms-git-*.pkg.tar.zst
# real pacman transaction: conflict without migration, success after it:
./tests/pacman-transaction.sh --package aic8800d80-dkms-git-*.pkg.tar.zst
```

`migration-from-install-sh.sh` builds a temporary root that looks like a
script-managed installation, puts stub `pacman`, `dkms` and `udevadm` in
`PATH`, sources the install scriptlet and calls `pre_install`/`post_install`
with `AIC8800_ROOT` pointing at that root. It checks that the old udev rule,
mode-switch data, firmware and DKMS source tree end up in the backup
byte-identical, that `dkms remove aic8800/1.0.0 --all` is called, that
package-owned paths are untouched, that only `99-aic8800.rules` remains after
the package is unpacked, and that the second run is a no-op.

`migration-e2e-bwrap.sh` repeats that scenario with the real `dkms` and the
real merged-`/usr` layout (`/lib` -> `/usr/lib`) inside a bwrap sandbox whose
`/etc`, `/var`, `/usr/src` and `/usr/lib/firmware` are private copies; it
exits 77 when bwrap or dkms is missing.

`pacman-transaction.sh` runs the real pacman against a private root (a bwrap
mount namespace with a copy of the local database and this driver's entry
removed) and covers the whole switch: plain `pacman -U` fails in the
file-conflict check with `pre_install` never running, `--dry-run` changes
nothing, `migrate-from-install-sh.sh` moves the leftovers, the same package
then installs, `post_install` reloads udev, and exactly one mode-switch rule
and one DKMS registration remain. It also checks that `--overwrite` installs
and that `pre_install` moves the firmware in that path. It exits 77 when bwrap
is missing; pacman 7's scriptlet network sandbox is disabled in the test
configuration because it cannot create a namespace inside the test sandbox.

The three scripts are installed under
`/usr/share/aic8800d80-dkms-git/tests/`; there they take the scriptlet from
the pacman database copy (`/var/lib/pacman/local/<pkg>/install`) or from
`/usr/share/aic8800d80-dkms-git/`, and the driver tree from
`/usr/src/aic8800-<pkgver>`.
