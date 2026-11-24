#!/bin/bash
# End-to-end check of the migration with the real dkms.
#
# migration-from-install-sh.sh proves the file handling with stub tools.  This
# script repeats the same scenario with a bwrap sandbox whose /etc, /var,
# /usr/src, /usr/lib/firmware and /usr/lib/udev/rules.d are private copies, so
# the real dkms removes a registration that the real dkms added, and the real
# merged-/usr layout (/lib -> /usr/lib) is exercised.  Nothing outside the
# temporary directory is written; pacman and udevadm are stubs because
# querying the live pacman database and triggering the host's udev would not
# describe a script-managed machine.
#
# Usage: tests/migration-e2e-bwrap.sh [--package <built package>] \
#            [--install-script <path>] [--repo-dir <driver tree>]
#
# Exits 77 when bwrap or dkms is not available.

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

while [ $# -gt 0 ]; do
    case "$1" in
        --package) PACKAGE=${2:?--package needs a path}; shift 2 ;;
        --install-script) INSTALL_SCRIPT=${2:?--install-script needs a path}; shift 2 ;;
        --repo-dir) REPO_DIR=${2:?--repo-dir needs a path}; shift 2 ;;
        -h | --help) sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) die "unknown argument: $1" ;;
    esac
done

command -v bwrap >/dev/null 2>&1 || { echo "skip: bwrap is not installed"; exit 77; }
command -v dkms >/dev/null 2>&1 || { echo "skip: dkms is not installed"; exit 77; }
[ -z "$PACKAGE" ] || [ -f "$PACKAGE" ] || die "package not found: $PACKAGE"

if [ -z "$REPO_DIR" ]; then
    for cand in "$SCRIPT_DIR/../../.." /usr/src/aic8800-*; do
        if [ -f "$cand/aic.rules" ] && [ -f "$cand/usb_modeswitch/1111_1111" ] && [ -f "$cand/dkms.conf" ]; then
            REPO_DIR=$cand
            break
        fi
    done
fi
[ -n "$REPO_DIR" ] || die "cannot find the driver tree; use --repo-dir"

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

W=$(mktemp -d "${TMPDIR:-/tmp}/aic8800-bwrap-e2e.XXXXXX")
trap 'rm -rf -- "$W"' EXIT
X=$W/root
STUB=$W/bin
mkdir -p "$X"/{etc/udev/rules.d,etc/usb_modeswitch.d,usr/src,usr/lib/firmware,usr/lib/udev/rules.d,usr/share/usb_modeswitch,var/backups,var/lib/dkms,tmp} "$STUB"

# A script-managed installation: what install.sh leaves behind.
cp "$REPO_DIR/aic.rules" "$X/etc/udev/rules.d/aic.rules"
cp "$REPO_DIR/usb_modeswitch/1111_1111" "$X/etc/usb_modeswitch.d/1111:1111"
cp -a "$REPO_DIR/fw/aic8800D80" "$X/usr/lib/firmware/"
install -dm755 "$X/usr/src/aic8800-1.0.0"
cp "$REPO_DIR/dkms.conf" "$X/usr/src/aic8800-1.0.0/dkms.conf"

# What the package installs.
if [ -n "$PACKAGE" ]; then
    bsdtar -xf "$PACKAGE" -C "$X"
else
    install -Dm644 "$REPO_DIR/aic.rules" "$X/usr/lib/udev/rules.d/99-aic8800.rules"
    sed -i 's#/etc/usb_modeswitch.d/1111:1111#/usr/share/usb_modeswitch/1111:1111#g' \
        "$X/usr/lib/udev/rules.d/99-aic8800.rules"
    install -Dm644 "$REPO_DIR/usb_modeswitch/1111_1111" "$X/usr/share/usb_modeswitch/1111:1111"
fi

# pacman and udevadm are stubs on purpose (see the header).
cat > "$STUB/pacman" <<'EOS'
#!/bin/bash
printf 'pacman %s\n' "$*" >> "$CALL_LOG"
exit 1   # nothing in this sandbox is owned by a package
EOS
cat > "$STUB/udevadm" <<'EOS'
#!/bin/bash
printf 'udevadm %s\n' "$*" >> "$CALL_LOG"
exit 0
EOS
chmod 755 "$STUB"/*
: > "$X/tmp/calls.log"

run() {
    bwrap --ro-bind / / \
        --bind "$X/etc" /etc \
        --bind "$X/var" /var \
        --bind "$X/usr/src" /usr/src \
        --bind "$X/usr/lib/firmware" /usr/lib/firmware \
        --bind "$X/usr/lib/udev/rules.d" /usr/lib/udev/rules.d \
        --bind "$X/usr/share/usb_modeswitch" /usr/share/usb_modeswitch \
        --bind "$X/tmp" /tmp \
        --dev /dev --proc /proc \
        --setenv TMPDIR /tmp \
        --setenv CALL_LOG /tmp/calls.log \
        --setenv PATH "$STUB:/usr/bin:/usr/sbin:/bin" \
        bash -c "$1"
}

failures=0
check() {
    local desc=$1
    shift
    if "$@"; then printf 'ok   - %s\n' "$desc"; else printf 'FAIL - %s\n' "$desc"; failures=$((failures + 1)); fi
}

echo "== real dkms ($(dkms --version | head -1)) adds the install.sh tree =="
run 'dkms add -m aic8800 -v 1.0.0 >/dev/null && dkms status'
check "dkms registered aic8800/1.0.0" test -d "$X/var/lib/dkms/aic8800/1.0.0"

echo
echo "== pre_install, then the package files, then post_install =="
run ". '$INSTALL_SCRIPT'; pre_install 1.0.0.r177.g1a2ce4e"
run 'dkms status > /tmp/dkms-status.txt 2>&1; cat /tmp/dkms-status.txt'

echo
echo "== assertions =="
backup=$(find "$X/var/backups" -maxdepth 1 -type d -name 'aic8800d80-migration-*' | head -n 1)
check "one backup directory" test -n "$backup"
check "the /etc udev rule was moved" test ! -e "$X/etc/udev/rules.d/aic.rules"
check "the backed-up rule is byte-identical" cmp -s "$REPO_DIR/aic.rules" "$backup/etc/udev/rules.d/aic.rules"
check "the modeswitch configuration was moved" test ! -e "$X/etc/usb_modeswitch.d/1111:1111"
check "the backed-up modeswitch configuration is byte-identical" \
    cmp -s "$REPO_DIR/usb_modeswitch/1111_1111" "$backup/etc/usb_modeswitch.d/1111:1111"
check "the firmware was moved out of /lib/firmware" test ! -e "$X/usr/lib/firmware/aic8800D80"
check "the firmware is complete in the backup" \
    cmp -s "$REPO_DIR/fw/aic8800D80/aic_userconfig_8800d80.txt" \
    "$backup/lib/firmware/aic8800D80/aic_userconfig_8800d80.txt"
check "the DKMS source tree was moved" test ! -e "$X/usr/src/aic8800-1.0.0"
check "the DKMS source tree is in the backup" test -f "$backup/usr/src/aic8800-1.0.0/dkms.conf"
check "the real dkms registration directory is gone" test ! -e "$X/var/lib/dkms/aic8800/1.0.0"
check "dkms status no longer lists aic8800/1.0.0" \
    bash -c '! grep -q "^aic8800/" "$1"' _ "$X/tmp/dkms-status.txt"
check "the packaged rule is installed" test -f "$X/usr/lib/udev/rules.d/99-aic8800.rules"
mapfile -t rules < <(grep -rl 'usb_modeswitch -b' "$X/etc/udev/rules.d" "$X/usr/lib/udev/rules.d" 2>/dev/null)
check "exactly one mode-switch rule remains" test "${#rules[@]}" -eq 1
check "it is the packaged rule" test "${rules[0]:-}" = "$X/usr/lib/udev/rules.d/99-aic8800.rules"
check "it points at /usr/share/usb_modeswitch" \
    grep -q '/usr/share/usb_modeswitch/1111:1111' "$X/usr/lib/udev/rules.d/99-aic8800.rules"

run ". '$INSTALL_SCRIPT'; post_install 1.0.0.r177.g1a2ce4e"
check "udevadm control --reload-rules was called" grep -qx 'udevadm control --reload-rules' "$X/tmp/calls.log"
check "only 1111:1111 was triggered" \
    grep -qx 'udevadm trigger --action=add --subsystem-match=usb --attr-match=idVendor=1111 --attr-match=idProduct=1111' "$X/tmp/calls.log"

printf '\n%d failure(s)\n' "$failures"
if [ "$failures" -ne 0 ]; then
    printf 'FAIL: bwrap end-to-end check failed\n' >&2
    exit 1
fi
printf 'PASS: migration with the real dkms and the real merged-/usr layout\n'
