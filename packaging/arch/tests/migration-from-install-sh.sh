#!/bin/bash
# Test the install.sh -> package migration performed by
# aic8800d80-dkms-git.install.
#
# The test builds a throw-away root, fills it with what a script-managed
# installation leaves behind, puts stub pacman/dkms/udevadm in PATH, sources
# the install scriptlet and calls pre_install/post_install with AIC8800_ROOT
# pointing at that root.  Nothing outside the temporary directory is touched:
# the stubs record their arguments instead of changing the system, so this can
# be run on a machine that already uses the driver.
#
# Usage:
#   tests/migration-from-install-sh.sh [--package <built package>] \
#       [--install-script <path>] [--repo-dir <driver source tree>]
#
#   --package  extract the built package into the temporary root with bsdtar
#              (the same extraction pacman does) between pre_install and
#              post_install, so the final state matches a real installation.
#              Without it the files package() installs are created directly.
#
# The scriptlet and the driver tree are found automatically in a repository
# checkout or on an installed system (the scriptlet comes from the pacman
# database copy at /var/lib/pacman/local/<pkg>/install, the tree from
# /usr/src/aic8800-*).

set -euo pipefail
trap 'printf "unexpected failure in %s at line %s\n" "${0##*/}" "$LINENO" >&2' ERR

die() {
    printf 'error: %s\n' "$*" >&2
    exit 2
}

SCRIPT_DIR=$(cd -- "$(dirname -- "$(readlink -f -- "$0")")" && pwd)
PACKAGE=""
INSTALL_SCRIPT=""
REPO_DIR="${AIC8800_REPO_DIR:-}"
PKGVER=""

while [ $# -gt 0 ]; do
    case "$1" in
        --package)
            [ $# -ge 2 ] || die "--package needs a path"
            PACKAGE=$2
            shift 2
            ;;
        --install-script)
            [ $# -ge 2 ] || die "--install-script needs a path"
            INSTALL_SCRIPT=$2
            shift 2
            ;;
        --repo-dir)
            [ $# -ge 2 ] || die "--repo-dir needs a path"
            REPO_DIR=$2
            shift 2
            ;;
        -h | --help)
            sed -n '2,24p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            die "unknown argument: $1"
            ;;
    esac
done

[ -z "$PACKAGE" ] || [ -f "$PACKAGE" ] || die "package not found: $PACKAGE"
[ -z "$INSTALL_SCRIPT" ] || [ -f "$INSTALL_SCRIPT" ] || die "install scriptlet not found: $INSTALL_SCRIPT"

if [ -z "$REPO_DIR" ]; then
    for cand in "$SCRIPT_DIR/../../.." /usr/src/aic8800-*; do
        if [ -f "$cand/aic.rules" ] && [ -f "$cand/usb_modeswitch/1111_1111" ] \
            && [ -f "$cand/dkms.conf" ]; then
            REPO_DIR=$cand
            break
        fi
    done
fi
[ -n "$REPO_DIR" ] || die "cannot find the driver tree (aic.rules, usb_modeswitch/1111_1111, dkms.conf); use --repo-dir"

if [ -z "$INSTALL_SCRIPT" ]; then
    if [ -f "$SCRIPT_DIR/../aic8800d80-dkms-git.install" ]; then
        INSTALL_SCRIPT="$SCRIPT_DIR/../aic8800d80-dkms-git.install"
    else
        for cand in /var/lib/pacman/local/aic8800d80-dkms-git-*/install; do
            if [ -f "$cand" ]; then
                INSTALL_SCRIPT=$cand
                break
            fi
        done
    fi
fi
[ -n "$INSTALL_SCRIPT" ] || die "cannot find aic8800d80-dkms-git.install; use --install-script"

if [ -n "$PACKAGE" ]; then
    PKGVER=${PACKAGE##*/}
    PKGVER=${PKGVER#aic8800d80-dkms-git-}
    PKGVER=${PKGVER%-*-any.pkg.tar.zst}
elif [ -f "$SCRIPT_DIR/../PKGBUILD" ]; then
    PKGVER=$(sed -n 's/^pkgver=//p' "$SCRIPT_DIR/../PKGBUILD")
else
    for cand in /usr/src/aic8800-*; do
        if [ -d "$cand" ]; then
            PKGVER=${cand##*/aic8800-}
            break
        fi
    done
fi
[ -n "$PKGVER" ] || die "cannot determine the package version"

WORK=$(mktemp -d "${TMPDIR:-/tmp}/aic8800-migration-test.XXXXXX")
trap 'rm -rf -- "$WORK"' EXIT

STUB_BIN=$WORK/bin
CALL_LOG=$WORK/calls.log
OWNED_LIST=$WORK/owned.txt
DKMS_STATE=$WORK/dkms-state.txt
BACKUP_ROOT=""
DKMS_REMOVE_FAIL=0
mkdir -p "$STUB_BIN"

cat > "$STUB_BIN/pacman" <<'STUB'
#!/bin/bash
printf 'pacman %s\n' "$*" >> "$CALL_LOG"
if [ "${1:-}" = "-Qqo" ]; then
    shift
    [ "${1:-}" = "--" ] && shift
    path=${1:-}
    while IFS= read -r owned; do
        [ -n "$owned" ] || continue
        if [ "$owned" = "$path" ]; then
            printf 'owned-pkg\n'
            exit 0
        fi
    done < "$OWNED_LIST"
    printf 'error: No package owns %s\n' "$path" >&2
    exit 1
fi
exit 1
STUB

cat > "$STUB_BIN/dkms" <<'STUB'
#!/bin/bash
printf 'dkms %s\n' "$*" >> "$CALL_LOG"
case "${1:-}" in
    status)
        [ -f "$DKMS_STATE" ] && cat "$DKMS_STATE"
        exit 0
        ;;
    remove)
        if [ "${DKMS_REMOVE_FAIL:-0}" = 1 ]; then
            printf 'Error! Could not remove module %s\n' "${2:-}" >&2
            exit 1
        fi
        ver=${2#aic8800/}
        sed -i "/^aic8800\/${ver}[,:]/d" "$DKMS_STATE"
        exit 0
        ;;
esac
exit 0
STUB

cat > "$STUB_BIN/udevadm" <<'STUB'
#!/bin/bash
printf 'udevadm %s\n' "$*" >> "$CALL_LOG"
exit 0
STUB
chmod 755 "$STUB_BIN"/*

checks=0
failures=0

check() {
    local desc=$1
    shift
    checks=$((checks + 1))
    if "$@"; then
        printf 'ok %2d - %s\n' "$checks" "$desc"
    else
        failures=$((failures + 1))
        printf 'not ok %2d - %s\n' "$checks" "$desc"
    fi
}

check_not() {
    local desc=$1
    shift
    checks=$((checks + 1))
    if "$@"; then
        failures=$((failures + 1))
        printf 'not ok %2d - %s\n' "$checks" "$desc"
    else
        printf 'ok %2d - %s\n' "$checks" "$desc"
    fi
}

reset_state() {
    : > "$CALL_LOG"
    : > "$OWNED_LIST"
    : > "$DKMS_STATE"
    DKMS_REMOVE_FAIL=0
    BACKUP_ROOT="$WORK/backups-$1"
    mkdir -p "$BACKUP_ROOT"
}

backup_dirs() {
    find "$BACKUP_ROOT" -mindepth 1 -maxdepth 1 -type d -name 'aic8800d80-migration-*' | sort
}

backup_count() {
    backup_dirs | wc -l | tr -d ' '
}

# What install.sh leaves behind on an Arch system.
make_installer_tree() {
    local root=$1
    mkdir -p "$root/etc/udev/rules.d" "$root/etc/usb_modeswitch.d" \
        "$root/lib/firmware/aic8800D80" "$root/lib/firmware/aic8800DC" \
        "$root/usr/src/aic8800-1.0.0" "$root/var/log"
    cp "$REPO_DIR/aic.rules" "$root/etc/udev/rules.d/aic.rules"
    cp "$REPO_DIR/usb_modeswitch/1111_1111" "$root/etc/usb_modeswitch.d/1111:1111"
    printf 'd80 firmware payload\n' > "$root/lib/firmware/aic8800D80/aic8800d80_fw.bin"
    printf 'dc firmware payload\n' > "$root/lib/firmware/aic8800DC/aic8800dc_fw.bin"
    sed 's/^PACKAGE_VERSION=.*/PACKAGE_VERSION="1.0.0"/; /^CLEAN=/d' \
        "$REPO_DIR/dkms.conf" > "$root/usr/src/aic8800-1.0.0/dkms.conf"
    printf 'installer noise\n' > "$root/usr/src/aic8800-1.0.0/README.md"
    printf 'aic8800/1.0.0, 6.17.0-test, x86_64: installed\n' > "$DKMS_STATE"
}

# What pacman puts on disk: either the built package or the same files.
install_package_files() {
    local root=$1 d
    if [ -n "$PACKAGE" ]; then
        bsdtar -xf "$PACKAGE" -C "$root"
        return 0
    fi
    install -Dm644 "$REPO_DIR/aic.rules" "$root/usr/lib/udev/rules.d/99-aic8800.rules"
    sed -i 's#/etc/usb_modeswitch.d/1111:1111#/usr/share/usb_modeswitch/1111:1111#g' \
        "$root/usr/lib/udev/rules.d/99-aic8800.rules"
    install -Dm644 "$REPO_DIR/usb_modeswitch/1111_1111" \
        "$root/usr/share/usb_modeswitch/1111:1111"
    install -dm755 "$root/usr/lib/firmware"
    for d in "$REPO_DIR"/fw/aic8800*; do
        cp -a "$d" "$root/usr/lib/firmware/"
    done
    install -dm755 "$root/usr/src/aic8800-$PKGVER"
    cp "$REPO_DIR/dkms.conf" "$root/usr/src/aic8800-$PKGVER/dkms.conf"
}

# pacman sources the scriptlet and calls one function with the new version.
run_hook() {
    local root=$1 hook=$2 log=$3
    env AIC8800_ROOT="$root" AIC8800_BACKUP_ROOT="$BACKUP_ROOT" \
        PATH="$STUB_BIN:$PATH" CALL_LOG="$CALL_LOG" OWNED_LIST="$OWNED_LIST" \
        DKMS_STATE="$DKMS_STATE" DKMS_REMOVE_FAIL="$DKMS_REMOVE_FAIL" \
        bash -c 'script=$1; hook=$2; ver=$3; shift 3; . "$script" || exit 1; "$hook" "$ver"' \
        bash "$INSTALL_SCRIPT" "$hook" "$PKGVER" > "$log" 2>&1
}

run_hook_ok() {
    local root=$1 hook=$2 log=$3
    checks=$((checks + 1))
    if run_hook "$root" "$hook" "$log"; then
        printf 'ok %2d - %s (%s) exits 0\n' "$checks" "$hook" "${root##*/}"
    else
        failures=$((failures + 1))
        printf 'not ok %2d - %s (%s) failed; output:\n' "$checks" "$hook" "${root##*/}"
        sed 's/^/      /' "$log"
    fi
}

test_migration_from_install_sh() {
    local root=$WORK/root-fresh backup rules=()
    printf '\n== migration from a script-managed installation ==\n'
    reset_state fresh
    make_installer_tree "$root"
    printf 'd80 firmware payload\n' > "$WORK/expected-d80.bin"

    run_hook_ok "$root" pre_install "$WORK/pre-install.log"
    printf -- '-- pre_install output --\n'
    sed 's/^/      /' "$WORK/pre-install.log"

    check "the /etc udev rule is gone" test ! -e "$root/etc/udev/rules.d/aic.rules"
    check "the /etc modeswitch configuration is gone" test ! -e "$root/etc/usb_modeswitch.d/1111:1111"
    check "the /lib/firmware/aic8800D80 tree is gone" test ! -e "$root/lib/firmware/aic8800D80"
    check "the /lib/firmware/aic8800DC tree is gone" test ! -e "$root/lib/firmware/aic8800DC"
    check "the /usr/src/aic8800-1.0.0 tree is gone" test ! -e "$root/usr/src/aic8800-1.0.0"

    check "exactly one backup directory was created" test "$(backup_count)" = 1
    backup=$(backup_dirs | head -n 1)
    check "the old udev rule was backed up" test -f "$backup/etc/udev/rules.d/aic.rules"
    check "the backed-up udev rule is byte-identical" \
        cmp -s "$REPO_DIR/aic.rules" "$backup/etc/udev/rules.d/aic.rules"
    check "the old modeswitch configuration was backed up" \
        cmp -s "$REPO_DIR/usb_modeswitch/1111_1111" "$backup/etc/usb_modeswitch.d/1111:1111"
    check "the old firmware was backed up byte-identical" \
        cmp -s "$WORK/expected-d80.bin" "$backup/lib/firmware/aic8800D80/aic8800d80_fw.bin"
    check "the old DKMS source tree was backed up" \
        test -f "$backup/usr/src/aic8800-1.0.0/dkms.conf"

    check "dkms remove aic8800/1.0.0 --all was called" \
        grep -qx 'dkms remove aic8800/1.0.0 --all' "$CALL_LOG"
    check_not "the aic8800/1.0.0 registration is gone" \
        grep -q '^aic8800/1\.0\.0[,:]' "$DKMS_STATE"

    install_package_files "$root"
    printf 'aic8800/%s, 6.17.0-test, x86_64: installed\n' "$PKGVER" >> "$DKMS_STATE"
    run_hook_ok "$root" post_install "$WORK/post-install.log"

    check "udevadm control --reload-rules was called" \
        grep -qx 'udevadm control --reload-rules' "$CALL_LOG"
    check "only the 1111:1111 add event was triggered" \
        grep -qx 'udevadm trigger --action=add --subsystem-match=usb --attr-match=idVendor=1111 --attr-match=idProduct=1111' "$CALL_LOG"
    check_not "no udev trigger without a device filter" \
        grep -qx 'udevadm trigger --action=add' "$CALL_LOG"

    check "the packaged 99-aic8800.rules is installed" \
        test -f "$root/usr/lib/udev/rules.d/99-aic8800.rules"
    check "the packaged rule points at /usr/share/usb_modeswitch" \
        grep -q '/usr/share/usb_modeswitch/1111:1111' "$root/usr/lib/udev/rules.d/99-aic8800.rules"
    check_not "the packaged rule no longer points at /etc/usb_modeswitch.d" \
        grep -q '/etc/usb_modeswitch.d/1111:1111' "$root/usr/lib/udev/rules.d/99-aic8800.rules"
    mapfile -t rules < <(grep -rl 'usb_modeswitch -b' "$root/etc/udev" "$root/usr/lib/udev" 2>/dev/null)
    check "exactly one mode-switch rule remains" test "${#rules[@]}" -eq 1
    check "it is the packaged 99-aic8800.rules" \
        test "${rules[0]:-}" = "$root/usr/lib/udev/rules.d/99-aic8800.rules"

    check "the package firmware is installed" \
        test -d "$root/usr/lib/firmware/aic8800D80"
    check "the package firmware directory is not empty" \
        test -n "$(find "$root/usr/lib/firmware/aic8800D80" -type f -print -quit)"
    check "the package DKMS source is installed" \
        test -f "$root/usr/src/aic8800-$PKGVER/dkms.conf"
    check "exactly one aic8800 DKMS registration remains" \
        test "$(sed -n '/^aic8800\//p' "$DKMS_STATE" | wc -l | tr -d ' ')" = 1
}

test_package_owned_paths_are_left_alone() {
    local root=$WORK/root-owned
    printf '\n== package-owned paths are never moved ==\n'
    reset_state owned
    make_installer_tree "$root"
    cat > "$OWNED_LIST" <<EOF
$root/etc/udev/rules.d/aic.rules
$root/etc/usb_modeswitch.d/1111:1111
$root/lib/firmware/aic8800D80
$root/lib/firmware/aic8800DC
$root/usr/src/aic8800-1.0.0
EOF

    run_hook_ok "$root" pre_install "$WORK/pre-owned.log"

    check "the owned udev rule is still there" test -f "$root/etc/udev/rules.d/aic.rules"
    check "the owned modeswitch configuration is still there" \
        test -f "$root/etc/usb_modeswitch.d/1111:1111"
    check "the owned firmware is still there" \
        test -f "$root/lib/firmware/aic8800D80/aic8800d80_fw.bin"
    check "the second owned firmware tree is still there" \
        test -f "$root/lib/firmware/aic8800DC/aic8800dc_fw.bin"
    check "the owned DKMS source tree is still there" \
        test -f "$root/usr/src/aic8800-1.0.0/dkms.conf"
    check "nothing was backed up" test "$(backup_count)" = 0
    check_not "no dkms remove was attempted" grep -q '^dkms remove' "$CALL_LOG"
    check "the aic8800/1.0.0 registration is untouched" \
        grep -q '^aic8800/1\.0\.0,' "$DKMS_STATE"
    check "the ownership check names the owning package" \
        grep -q 'owned by owned-pkg' "$WORK/pre-owned.log"
    check "the remaining /etc rule is reported" \
        grep -q '/etc/udev/rules.d/aic.rules is still present' "$WORK/pre-owned.log"
}

test_second_run_is_a_no_op() {
    local root=$WORK/root-idempotent
    printf '\n== a second run is a no-op ==\n'
    reset_state idempotent
    make_installer_tree "$root"

    run_hook_ok "$root" pre_install "$WORK/pre-idem-1.log"
    run_hook_ok "$root" pre_install "$WORK/pre-idem-2.log"

    check "still exactly one backup directory" test "$(backup_count)" = 1
    check "the second run reports nothing to migrate" \
        grep -q 'nothing to migrate' "$WORK/pre-idem-2.log"
    check "dkms remove was called exactly once" \
        test "$(sed -n '/^dkms remove/p' "$CALL_LOG" | wc -l | tr -d ' ')" = 1

    # pre_upgrade/post_upgrade share the implementation; make sure they are
    # wired up and just as harmless on a second run.
    run_hook_ok "$root" pre_upgrade "$WORK/pre-idem-3.log"
    check "pre_upgrade reports nothing to migrate" \
        grep -q 'nothing to migrate' "$WORK/pre-idem-3.log"
    check "pre_upgrade created no second backup directory" test "$(backup_count)" = 1
    run_hook_ok "$root" post_upgrade "$WORK/post-idem.log"
    check "post_upgrade reloads the udev rules" \
        test "$(sed -n '/^udevadm control --reload-rules$/p' "$CALL_LOG" | wc -l | tr -d ' ')" = 1
}

test_dkms_failure_is_reported() {
    local root=$WORK/root-dkmsfail
    printf '\n== a failing dkms remove is reported, not swallowed ==\n'
    reset_state dkmsfail
    make_installer_tree "$root"
    DKMS_REMOVE_FAIL=1
    run_hook_ok "$root" pre_install "$WORK/pre-dkmsfail.log"
    DKMS_REMOVE_FAIL=0

    check "the failure is reported on stderr" \
        grep -q 'warning: dkms remove aic8800/1.0.0 --all failed' "$WORK/pre-dkmsfail.log"
    check "the source tree is kept while the registration exists" \
        test -f "$root/usr/src/aic8800-1.0.0/dkms.conf"
    check "the registration is still listed" \
        grep -q '^aic8800/1\.0\.0,' "$DKMS_STATE"
    check "the independent firmware migration still happened" \
        test ! -e "$root/lib/firmware/aic8800D80"
    check "the firmware is still in the backup" \
        test -f "$(backup_dirs | head -n 1)/lib/firmware/aic8800D80/aic8800d80_fw.bin"
}

test_foreign_files_are_not_moved() {
    local root=$WORK/root-foreign
    printf '\n== files that are not this driver are left alone ==\n'
    reset_state foreign
    mkdir -p "$root/etc/udev/rules.d" "$root/etc/usb_modeswitch.d"
    printf '# unrelated rules\nACTION=="add", RUN+="/bin/true"\n' \
        > "$root/etc/udev/rules.d/aic.rules"
    printf 'DefaultVendor=0x1111\n' > "$root/etc/usb_modeswitch.d/1111:1111"

    run_hook_ok "$root" pre_install "$WORK/pre-foreign.log"

    check "the foreign aic.rules is kept" test -f "$root/etc/udev/rules.d/aic.rules"
    check "the foreign modeswitch configuration is kept" \
        test -f "$root/etc/usb_modeswitch.d/1111:1111"
    check "no backup directory was created" test "$(backup_count)" = 0
}

printf 'install scriptlet: %s\n' "$INSTALL_SCRIPT"
printf 'driver tree:       %s\n' "$REPO_DIR"
printf 'package:           %s\n' "${PACKAGE:-(not supplied; creating the packaged files directly)}"
printf 'package version:   %s\n' "$PKGVER"
printf 'temporary root:    %s\n' "$WORK"

test_migration_from_install_sh
test_package_owned_paths_are_left_alone
test_second_run_is_a_no_op
test_dkms_failure_is_reported
test_foreign_files_are_not_moved

printf '\n%d checks, %d failures\n' "$checks" "$failures"
if [ "$failures" -ne 0 ]; then
    printf 'FAIL: %d check(s) failed\n' "$failures" >&2
    exit 1
fi
printf 'PASS: the install.sh migration and udev handling behave as expected\n'
