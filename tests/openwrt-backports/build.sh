#!/usr/bin/env bash
# Compile a source copy against a prepared kernel and staged wireless backport.
# Leaves the build directory for inspection; does not install or load modules.
set -euo pipefail

if [ "$#" -lt 4 ]; then
    echo "Usage: $0 KERNEL_BUILD_DIR STAGED_INCLUDE_DIR MAC80211_SYMVERS CFG80211_VERSION [make arguments...]" >&2
    exit 2
fi

kernel_dir=$(realpath "$1")
include_dir=$(realpath "$2")
symvers=$(realpath "$3")
wireless_version=$4
shift 4

for required in "$kernel_dir/Makefile" "$kernel_dir/Module.symvers" \
    "$include_dir/mac80211/net/cfg80211.h" \
    "$include_dir/mac80211-backport/backport/autoconf.h" "$symvers"; do
    if [ ! -f "$required" ]; then
        echo "Missing build input: $required" >&2
        exit 2
    fi
done

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
build_dir=$(mktemp -d "${TMPDIR:-/tmp}/aic8800-backports.XXXXXX")
cp -a "$repo_dir/drivers/aic8800/." "$build_dir/"
echo "Build directory: $build_dir"

make -C "$kernel_dir" M="$build_dir" \
    NOSTDINC_FLAGS="-I$include_dir/mac80211-backport/uapi -I$include_dir/mac80211-backport -I$include_dir/mac80211/uapi -I$include_dir/mac80211 -include backport/backport.h" \
    KBUILD_EXTRA_SYMBOLS="$symvers" \
    CFG80211_VERSION="$wireless_version" \
    -j"${JOBS:-2}" "$@" modules

for module in aic_load_fw/aic_load_fw.ko aic8800_fdrv/aic8800_fdrv.ko \
    aic_zlp_quirk/aic_zlp_quirk.ko; do
    test -s "$build_dir/$module"
done
echo "All three modules built successfully in $build_dir"
