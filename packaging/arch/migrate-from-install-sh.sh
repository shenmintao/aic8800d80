#!/bin/bash
# Move a script-managed (install.sh) installation out of the way so that this
# package can be installed with a plain pacman -U.
#
# Why this script exists: pacman checks for file conflicts in
# _alpm_sync_check() before it commits a transaction, and the install
# scriptlet's pre_install runs inside the commit.  install.sh leaves the
# firmware in /lib/firmware/aic8800*, which is /usr/lib/firmware/aic8800* on
# merged-/usr systems, so the package's own firmware files already exist
# unowned when pacman compares file lists.  pacman then aborts with
# "exists in filesystem" and pre_install never runs.  This script performs the
# migration first, from outside pacman.
#
# The migration itself is the same code pacman's pre_install uses: this script
# sources aic8800d80-dkms-git.install from its own directory and calls
# _aic_migrate_from_installer, so both paths cannot drift apart.
#
# Usage:
#   sudo ./migrate-from-install-sh.sh [--dry-run] [--yes] [--backup-root DIR]
#
#   --dry-run          print what would be moved or removed, change nothing
#   --yes              do not ask for confirmation
#   --backup-root DIR  use DIR instead of /var/backups
#   --install-script   path to aic8800d80-dkms-git.install (default: next to
#                      this script)
#
# Unowned files only: a path owned by any package (pacman -Qqo) is never moved
# or removed.  Nothing is deleted; everything goes to
# <backup-root>/aic8800d80-migration-<timestamp>/ with its absolute path.

set -uo pipefail

script_dir=$(cd -- "$(dirname -- "$(readlink -f -- "$0")")" && pwd)
install_script="$script_dir/aic8800d80-dkms-git.install"
backup_root=""
dry_run=0
assume_yes=0

die() {
    printf 'error: %s\n' "$*" >&2
    exit 2
}

usage() {
    sed -n '2,32p' "$0" | sed 's/^# \{0,1\}//'
}

while [ $# -gt 0 ]; do
    case "$1" in
        --dry-run) dry_run=1; shift ;;
        --yes | -y) assume_yes=1; shift ;;
        --backup-root)
            [ $# -ge 2 ] || die "--backup-root needs a directory"
            backup_root=$2
            shift 2
            ;;
        --install-script)
            [ $# -ge 2 ] || die "--install-script needs a path"
            install_script=$2
            shift 2
            ;;
        -h | --help) usage; exit 0 ;;
        *) die "unknown argument: $1 (use --help)" ;;
    esac
done

[ -f "$install_script" ] || die "cannot find $(basename "$install_script") next to this script; pass --install-script"
[ -z "$backup_root" ] || [ -d "$backup_root" ] || die "backup root does not exist: $backup_root"

if [ "$dry_run" -eq 0 ] && [ "$(id -u)" -ne 0 ]; then
    die "run as root (sudo $0), or use --dry-run"
fi

# Load the migration functions.  Sourcing runs only variable assignments.
# shellcheck source=aic8800d80-dkms-git.install
. "$install_script" || die "cannot load $install_script"

_aic_dry_run=$dry_run
[ -z "$backup_root" ] || AIC8800_BACKUP_ROOT=$backup_root

printf 'Migrating a script-managed AIC8800D80 installation.\n'
printf '  udev rule        /etc/udev/rules.d/aic.rules\n'
printf '  modeswitch data  /etc/usb_modeswitch.d/1111:1111\n'
printf '  firmware         /lib/firmware/aic8800*\n'
printf '  DKMS source      /usr/src/aic8800-%s\n' "$_aic_dkms_version"
printf '  DKMS entry       aic8800/%s\n' "$_aic_dkms_version"
printf 'Only paths that no package owns are touched; files are moved to a backup, not deleted.\n'
[ "$dry_run" -eq 0 ] || printf 'Dry run: nothing will be changed.\n'
printf '\n'

if [ "$dry_run" -eq 0 ] && [ "$assume_yes" -eq 0 ]; then
    if [ ! -t 0 ]; then
        die "no terminal to ask for confirmation; pass --yes"
    fi
    answer=""
    read -r -p "Continue? (y/N): " answer || answer="n"
    case "$answer" in
        [yY]) ;;
        *) printf 'Cancelled; nothing was changed.\n'; exit 1 ;;
    esac
fi

_aic_migrate_from_installer

if [ "$dry_run" -eq 1 ]; then
    printf 'Dry run finished; nothing was changed.\n'
    exit 0
fi

# Post-condition: every path that was ours to move must be gone, and the
# registration must be gone when its tree was ours.  A failure here means the
# package would still hit the file conflict, so do not report success.
leftovers=0
_report_leftover() {
    printf 'still present: %s\n' "$1" >&2
    leftovers=$((leftovers + 1))
}
for _p in /etc/udev/rules.d/aic.rules \
          /etc/usb_modeswitch.d/1111:1111 \
          /usr/src/aic8800-"$_aic_dkms_version"; do
    if [ -e "$_p" ] && [ -z "$(_aic_owners "$_p")" ]; then
        _report_leftover "$_p"
    fi
done
for _p in /lib/firmware/aic8800* /usr/lib/firmware/aic8800*; do
    [ -e "$_p" ] || continue
    if [ -z "$(_aic_owners "$_p")" ]; then
        _report_leftover "$_p"
    fi
done
if command -v dkms >/dev/null 2>&1 \
    && dkms status 2>/dev/null | grep -q "^aic8800/${_aic_dkms_version}[,:]" \
    && [ -z "$(_aic_owners "/usr/src/aic8800-$_aic_dkms_version")" ]; then
    _report_leftover "dkms status: aic8800/${_aic_dkms_version}"
fi

if [ "$leftovers" -ne 0 ]; then
    printf '\nMigration incomplete: %d item(s) above are still in place.\n' "$leftovers" >&2
    printf 'Fix the errors printed above and run this script again.\n' >&2
    exit 1
fi

printf '\nDone. The package can now be installed with:\n'
printf '  sudo pacman -U aic8800d80-dkms-git-*.pkg.tar.zst\n'
exit 0
