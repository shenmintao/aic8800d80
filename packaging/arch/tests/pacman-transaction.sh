#!/bin/bash
# Real pacman transaction test for the install.sh -> package switch.
#
# migration-from-install-sh.sh proves the file handling with stub tools.  This
# script runs the real pacman against a private root and covers the three
# paths a script-managed machine can take:
#
#   1. plain `pacman -U` fails in the file-conflict check ("exists in
#      filesystem") and pre_install never runs, because _alpm_sync_check()
#      compares file lists before the transaction is committed;
#   2. after migrate-from-install-sh.sh has moved the unowned firmware (and the
#      other leftovers), the same package installs, post_install reloads udev,
#      and only 99-aic8800.rules and the package's own DKMS registration
#      remain;
#   3. `pacman -U --overwrite '/usr/lib/firmware/aic8800*'` also installs,
#      because --overwrite reaches the commit and pre_install moves the old
#      firmware before extraction.
#
# pacman 7 runs scriptlets in a network namespace and aborts them when it
# cannot create one (unprivileged containers, which this sandbox is), so the
# test config sets DisableSandboxNetwork; the scriptlets are otherwise run
# unchanged.
#
# The root is a bwrap mount namespace: /etc/udev/rules.d, /etc/usb_modeswitch.d,
# /var, /usr/src, /usr/lib/firmware, /usr/lib/udev/rules.d and /usr/share are
# private copies, pacman runs with `--root /` inside it (scriptlets run
# un-chrooted, exactly as in a normal install), and pacman hooks are disabled
# because the dkms hook would build the module for the running kernel.  The
# pacman database is a copy of the host's with this driver's package entry
# removed, so the installer files are unowned as they are on a script-managed
# machine.  Nothing outside the temporary directory is written.
#
# Usage: tests/pacman-transaction.sh --package <built package>
#
# Exits 77 when bwrap is not available.

set -euo pipefail
trap 'printf "unexpected failure in %s at line %s\n" "${0##*/}" "$LINENO" >&2' ERR

die() {
    printf 'error: %s\n' "$*" >&2
    exit 2
}

SCRIPT_DIR=$(cd -- "$(dirname -- "$(readlink -f -- "$0")")" && pwd)
PACKAGE=""
REPO_DIR="${AIC8800_REPO_DIR:-}"
MIGRATE_SCRIPT=""

while [ $# -gt 0 ]; do
    case "$1" in
        --package) PACKAGE=${2:?--package needs a path}; shift 2 ;;
        --repo-dir) REPO_DIR=${2:?--repo-dir needs a path}; shift 2 ;;
        --migrate-script) MIGRATE_SCRIPT=${2:?--migrate-script needs a path}; shift 2 ;;
        -h | --help) sed -n '2,33p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) die "unknown argument: $1" ;;
    esac
done

command -v bwrap >/dev/null 2>&1 || { echo "skip: bwrap is not installed"; exit 77; }
[ -n "$PACKAGE" ] || die "--package <built package> is required"
[ -f "$PACKAGE" ] || die "package not found: $PACKAGE"

if [ -z "$REPO_DIR" ]; then
    for cand in "$SCRIPT_DIR/../../.." /usr/src/aic8800-*; do
        if [ -f "$cand/aic.rules" ] && [ -f "$cand/usb_modeswitch/1111_1111" ] && [ -f "$cand/dkms.conf" ]; then
            REPO_DIR=$cand
            break
        fi
    done
fi
[ -n "$REPO_DIR" ] || die "cannot find the driver tree; use --repo-dir"

if [ -z "$MIGRATE_SCRIPT" ] && [ -f "$SCRIPT_DIR/../migrate-from-install-sh.sh" ]; then
    MIGRATE_SCRIPT="$SCRIPT_DIR/../migrate-from-install-sh.sh"
fi
[ -z "$MIGRATE_SCRIPT" ] || [ -f "$MIGRATE_SCRIPT" ] || die "migrate script not found: $MIGRATE_SCRIPT"
[ -n "$MIGRATE_SCRIPT" ] || die "cannot find migrate-from-install-sh.sh; use --migrate-script"

PKGVER=${PACKAGE##*/}
PKGVER=${PKGVER#aic8800d80-dkms-git-}
PKGVER=${PKGVER%-*-any.pkg.tar.zst}

W=$(mktemp -d "${TMPDIR:-/tmp}/aic8800-pacman.XXXXXX")
trap 'rm -rf -- "$W"' EXIT

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

# A disposable root that looks like a machine where install.sh ran.
new_root() {
    local name=$1 root=$W/$1
    mkdir -p "$root"/{etc/udev/rules.d,etc/usb_modeswitch.d,src,fw,udevrules,usrshare} \
        "$root"/{var/lib/pacman,var/lib/dkms,var/cache/pacman/pkg,var/backups,var/log,var/emptyhooks,tmp/testbin}
    cp -a /var/lib/pacman/local "$root/var/lib/pacman/local"
    rm -rf "$root/var/lib/pacman/local"/aic8800d80-dkms-local-*
    printf '[options]\nHoldPkg = pacman glibc\nArchitecture = auto\nSigLevel = Never\nDisableSandboxNetwork\nHookDir = /var/emptyhooks\n' \
        > "$root/tmp/pacman.conf"
    cp "$REPO_DIR/aic.rules" "$root/etc/udev/rules.d/aic.rules"
    cp "$REPO_DIR/usb_modeswitch/1111_1111" "$root/etc/usb_modeswitch.d/1111:1111"
    cp -a "$REPO_DIR/fw/aic8800D80" "$root/fw/"
    install -dm755 "$root/src/aic8800-1.0.0"
    cp "$REPO_DIR/dkms.conf" "$root/src/aic8800-1.0.0/dkms.conf"
    cat > "$root/tmp/testbin/udevadm" <<'EOS'
#!/bin/bash
printf 'udevadm %s\n' "$*" >> /tmp/calls.log
exit 0
EOS
    chmod 755 "$root/tmp/testbin/udevadm"
    : > "$root/tmp/calls.log"
}

run_in() {
    local root=$1
    shift
    bwrap --unshare-user --uid 0 --gid 0 \
        --ro-bind / / \
        --tmpfs /etc \
        --bind "$root/etc/udev/rules.d" /etc/udev/rules.d \
        --bind "$root/etc/usb_modeswitch.d" /etc/usb_modeswitch.d \
        --bind "$root/var" /var \
        --bind "$root/src" /usr/src \
        --bind "$root/fw" /usr/lib/firmware \
        --bind "$root/udevrules" /usr/lib/udev/rules.d \
        --bind "$root/usrshare" /usr/share \
        --bind "$root/tmp/testbin/udevadm" /usr/bin/udevadm \
        --bind "$root/tmp" /tmp \
        --dev /dev --proc /proc \
        --setenv PATH /tmp/testbin:/usr/bin:/usr/sbin:/bin \
        --setenv TMPDIR /tmp \
        bash -c "$*"
}

# Run a command in the root, keep the output in <log> and the exit code in
# <log>.rc so that both can be asserted.
run_capture() {
    local root=$1 log=$2
    shift 2
    local rc=0
    run_in "$root" "$*" > "$log" 2>&1 || rc=$?
    printf '%s\n' "$rc" > "$log.rc"
}

rc_of() {
    cat "$1.rc"
}

pacman_u() {
    local root=$1 log=$2
    shift 2
    run_capture "$root" "$log" "pacman --config /tmp/pacman.conf -U --noconfirm --root / \
        --dbpath /var/lib/pacman --cachedir /var/cache/pacman/pkg \
        --hookdir /var/emptyhooks $* '$PACKAGE'"
}

backup_count() {
    find "$1/var/backups" -mindepth 1 -maxdepth 1 -type d -name 'aic8800d80-migration-*' 2>/dev/null | wc -l | tr -d ' '
}

mode_switch_rules() {
    grep -rl 'usb_modeswitch -b' "$1/etc/udev/rules.d" "$1/udevrules" 2>/dev/null | wc -l | tr -d ' '
}

echo "package:         ${PACKAGE##*/}"
echo "package version: $PKGVER"
echo "driver tree:     $REPO_DIR"
echo "migrate script:  $MIGRATE_SCRIPT"

# ---------------------------------------------------------------------------
# 1. Plain pacman -U must stop at the file-conflict check.
# ---------------------------------------------------------------------------
echo
echo "== 1. pacman -U without migration =="
A=$W/a
new_root a
run_capture "$A" "$W/dkms-a0.log" 'dkms add -m aic8800 -v 1.0.0 >/dev/null 2>&1 && dkms status'
check "the root starts with an aic8800/1.0.0 registration" \
    grep -q '^aic8800/1\.0\.0[,:]' "$W/dkms-a0.log"

pacman_u "$A" "$W/pacman-a1.log"
echo "-- pacman exit code: $(rc_of "$W/pacman-a1.log") --"
tail -n 6 "$W/pacman-a1.log" | sed 's/^/    /'

check "pacman -U exits non-zero" test "$(rc_of "$W/pacman-a1.log")" -ne 0
check "pacman reports conflicting files" \
    grep -q 'failed to commit transaction (conflicting files)' "$W/pacman-a1.log"
check "the conflict names the packaged firmware" \
    grep -q 'usr/lib/firmware/aic8800D80/.* exists in filesystem' "$W/pacman-a1.log"
check "pre_install did not run (no backup directory)" test "$(backup_count "$A")" = 0
check "the /etc udev rule was left in place" test -f "$A/etc/udev/rules.d/aic.rules"
check "the firmware was left in place" test -f "$A/fw/aic8800D80/fw_patch_8800d80_u02.bin"
check "the DKMS source tree was left in place" test -f "$A/src/aic8800-1.0.0/dkms.conf"
run_capture "$A" "$W/dkms-a1.log" 'dkms status'
check "the aic8800/1.0.0 registration was left in place" \
    grep -q '^aic8800/1\.0\.0[,:]' "$W/dkms-a1.log"
check ""pre_install" output is absent from the failed transaction" \
    bash -c "! grep -q 'checking for files left by install.sh' '$W/pacman-a1.log'"
check "the packaged udev rule was not installed" test ! -e "$A/udevrules/99-aic8800.rules"
check "the package is not in the database" \
    test ! -e "$A/var/lib/pacman/local/aic8800d80-dkms-git-$PKGVER"

# ---------------------------------------------------------------------------
# 2. Migrate first, then the same package installs.
# ---------------------------------------------------------------------------
echo
echo "== 2. migrate-from-install-sh.sh --yes, then pacman -U =="
run_capture "$A" "$W/migrate-dry.log" "'$MIGRATE_SCRIPT' --yes --dry-run"
echo "-- dry-run exit code: $(rc_of "$W/migrate-dry.log") --"
tail -n 4 "$W/migrate-dry.log" | sed 's/^/    /'
check "the dry run exits 0" test "$(rc_of "$W/migrate-dry.log")" = 0
check "the dry run reports the files it would move" \
    grep -q 'would move /etc/udev/rules.d/aic.rules' "$W/migrate-dry.log"
check "the dry run changes nothing (no backup directory)" test "$(backup_count "$A")" = 0
check "the dry run keeps the /etc rule" test -f "$A/etc/udev/rules.d/aic.rules"
run_capture "$A" "$W/dkms-dry.log" 'dkms status'
check "the dry run keeps the DKMS registration" grep -q '^aic8800/1\.0\.0[,:]' "$W/dkms-dry.log"

run_capture "$A" "$W/migrate-a.log" "'$MIGRATE_SCRIPT' --yes"
echo "-- migrate exit code: $(rc_of "$W/migrate-a.log") --"
tail -n 10 "$W/migrate-a.log" | sed 's/^/    /'

check "the migration script exits 0" test "$(rc_of "$W/migrate-a.log")" = 0
check "it moved files into the backup directory" test "$(backup_count "$A")" = 1
check "the /etc udev rule is gone" test ! -e "$A/etc/udev/rules.d/aic.rules"
check "the firmware is gone from /lib/firmware" test ! -e "$A/fw/aic8800D80"
check "the DKMS source tree is gone" test ! -e "$A/src/aic8800-1.0.0"
run_capture "$A" "$W/dkms-a2.log" 'dkms status'
check_not "the aic8800/1.0.0 registration is gone" \
    grep -q '^aic8800/1\.0\.0[,:]' "$W/dkms-a2.log"
check "the backup keeps the firmware files" \
    bash -c "ls '$A'/var/backups/aic8800d80-migration-*/lib/firmware/aic8800D80/fw_patch_8800d80_u02.bin >/dev/null"

pacman_u "$A" "$W/pacman-a2.log"
echo "-- pacman exit code: $(rc_of "$W/pacman-a2.log") --"
tail -n 6 "$W/pacman-a2.log" | sed 's/^/    /'

check "pacman -U now exits 0" test "$(rc_of "$W/pacman-a2.log")" = 0
check "the package was installed" grep -q 'installing aic8800d80-dkms-git' "$W/pacman-a2.log"
check "pre_install ran and found nothing left to move" \
    grep -q 'aic8800d80: nothing to migrate' "$W/pacman-a2.log"
check "the package files under /usr/share are installed" \
    test -f "$A/usrshare/aic8800d80-dkms-git/tests/pacman-transaction.sh"
check "the packaged udev rule is installed" test -f "$A/udevrules/99-aic8800.rules"
check "it points at /usr/share/usb_modeswitch" \
    grep -q '/usr/share/usb_modeswitch/1111:1111' "$A/udevrules/99-aic8800.rules"
check "exactly one mode-switch rule is loaded" test "$(mode_switch_rules "$A")" = 1
check "the package firmware is installed" test -f "$A/fw/aic8800D80/fw_patch_8800d80_u02.bin"
check "the package DKMS source is installed" test -f "$A/src/aic8800-$PKGVER/dkms.conf"
check "post_install reloaded the udev rules" grep -qx 'udevadm control --reload-rules' "$A/tmp/calls.log"
check "post_install triggered only 1111:1111" \
    grep -qx 'udevadm trigger --action=add --subsystem-match=usb --attr-match=idVendor=1111 --attr-match=idProduct=1111' "$A/tmp/calls.log"

# The dkms pacman hook would register the module after the transaction; hooks
# are disabled here, so add the registration by hand and check that the old
# one does not come back.
run_capture "$A" "$W/dkms-a3.log" "dkms add -m aic8800 -v '$PKGVER' >/dev/null 2>&1 && dkms status"
sed 's/^/    /' "$W/dkms-a3.log"
check "only the package's DKMS registration remains" \
    test "$(sed -n '/^aic8800\//p' "$W/dkms-a3.log" | wc -l | tr -d ' ')" = 1
check "that registration is aic8800/$PKGVER" \
    grep -q "^aic8800/$PKGVER[,:]" "$W/dkms-a3.log"

# ---------------------------------------------------------------------------
# 3. --overwrite is the documented alternative.
# ---------------------------------------------------------------------------
echo
echo "== 3. pacman -U --overwrite '/usr/lib/firmware/aic8800*' without migration =="
B=$W/b
new_root b
run_capture "$B" "$W/dkms-b0.log" 'dkms add -m aic8800 -v 1.0.0 >/dev/null 2>&1 && dkms status'
pacman_u "$B" "$W/pacman-b1.log" "--overwrite '/usr/lib/firmware/aic8800*'"
echo "-- pacman exit code: $(rc_of "$W/pacman-b1.log") --"
tail -n 4 "$W/pacman-b1.log" | sed 's/^/    /'

check "--overwrite installs the package" test "$(rc_of "$W/pacman-b1.log")" = 0
check "--overwrite still runs pre_install (one backup directory)" test "$(backup_count "$B")" = 1
check "--overwrite lets pre_install move the firmware before extraction" \
    grep -q 'aic8800d80: moved 4 item(s)' "$W/pacman-b1.log"
check "--overwrite leaves the /etc rule migrated away" test ! -e "$B/etc/udev/rules.d/aic.rules"
check "only the packaged rule remains" test "$(mode_switch_rules "$B")" = 1
run_capture "$B" "$W/dkms-b1.log" 'dkms status'
check_not "the old DKMS registration is gone" grep -q '^aic8800/1\.0\.0[,:]' "$W/dkms-b1.log"

printf '\n%d checks, %d failures\n' "$checks" "$failures"
if [ "$failures" -ne 0 ]; then
    printf 'FAIL: %d check(s) failed\n' "$failures" >&2
    exit 1
fi
printf 'PASS: real pacman transaction: conflict without migration, success after it\n'
