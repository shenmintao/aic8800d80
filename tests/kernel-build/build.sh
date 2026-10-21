#!/usr/bin/env bash
# Build a source copy against a prepared native kernel; never install or load it.
set -euo pipefail

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 KERNEL_BUILD_DIR [make arguments...]" >&2
    exit 2
fi

kernel_dir=$(realpath "$1")
shift

for required in "$kernel_dir/Makefile" "$kernel_dir/Module.symvers" \
    "$kernel_dir/include/generated/autoconf.h"; do
    if [ ! -s "$required" ]; then
        echo "Missing build input: $required" >&2
        exit 2
    fi
done

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
build_dir=$(mktemp -d "${TMPDIR:-/tmp}/aic8800-kernel-build.XXXXXX")
cp -a "$repo_dir/drivers/aic8800/." "$build_dir/"
echo "Build directory: $build_dir"

# Discard copied build outputs so stale modules cannot satisfy the check.
make -C "$kernel_dir" M="$build_dir" "$@" clean
make -C "$kernel_dir" M="$build_dir" -j"${JOBS:-2}" "$@" modules

for module in aic_load_fw/aic_load_fw.ko aic8800_fdrv/aic8800_fdrv.ko \
    aic_zlp_quirk/aic_zlp_quirk.ko; do
    test -s "$build_dir/$module"
done

echo "All three modules built successfully in $build_dir"
