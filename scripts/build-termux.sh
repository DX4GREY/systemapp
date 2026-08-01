#!/usr/bin/env bash
# Build Termux-compatible .deb packages around the already-built Android
# binaries. Supports every ABI Termux officially targets:
#   arm64-v8a   -> aarch64
#   armeabi-v7a -> arm
#   x86         -> i686
#   x86_64      -> x86_64
#
# Usage:
#   ABI=arm64-v8a ./build.sh termux   # build one architecture
#   ABI=all ./build.sh termux         # build all four (default)
#
# Does NOT recompile from source, since doing that on a non-Android
# host would silently package a host-architecture binary labeled with the
# wrong Debian architecture (that was a bug in an earlier version of this
# script - packaging must reuse the actual cross-compiled Android binary,
# not a host rebuild).
set -euo pipefail
cd "$(dirname "$0")/.."

VERSION="$(grep -oP '(?<=kVersion = ")[^"]+' include/systemapp/version.hpp)"
PKG_BASE="build-termux"
TERMUX_PREFIX="${PREFIX:-/data/data/com.termux/files/usr}"

# Map Android ABI name -> Termux/Debian architecture name.
abi_to_deb_arch() {
    case "$1" in
        arm64-v8a)   echo aarch64 ;;
        armeabi-v7a) echo arm ;;
        x86)         echo i686 ;;
        x86_64)      echo x86_64 ;;
        *) echo "error: unsupported ABI '$1' (expected arm64-v8a, armeabi-v7a, x86, x86_64)" >&2; exit 1 ;;
    esac
}

build_one() {
    local abi="$1"
    local deb_arch
    deb_arch="$(abi_to_deb_arch "${abi}")"

    local bin_src="release/systemapp-${abi}"
    if [[ ! -f "${bin_src}" ]]; then
        echo "error: ${bin_src} not found - run 'ABI=${abi} ./build.sh binary' first" >&2
        exit 1
    fi

    local pkg_root="${PKG_BASE}/${deb_arch}/pkgroot"
    local bin_dir="${pkg_root}${TERMUX_PREFIX}/bin"

    rm -rf "${pkg_root}"
    mkdir -p "${bin_dir}" "${pkg_root}/DEBIAN"

    cp "${bin_src}" "${bin_dir}/systemapp"
    chmod 755 "${bin_dir}/systemapp"

    cat > "${pkg_root}/DEBIAN/control" <<EOF
Package: systemapp
Version: ${VERSION}
Architecture: ${deb_arch}
Maintainer: SystemApp Project
Description: Native Android system administration CLI (root-only operations)
 SystemApp is a BusyBox/Toybox-style native CLI for managing system apps,
 partitions, properties, and root-level Android system state.
EOF

    mkdir -p release
    # --root-owner-group so files are owned by root in the package even when
    # building as a non-root user (prevents the "unusual owner" warning).
    dpkg-deb --root-owner-group --build "${pkg_root}" "release/systemapp-${deb_arch}.deb"
    echo "Built release/systemapp-${deb_arch}.deb"
}

ABI="${ABI:-all}"

if [[ "${ABI}" == "all" ]]; then
    for abi in arm64-v8a armeabi-v7a x86 x86_64; do
        build_one "${abi}"
    done
else
    build_one "${ABI}"
fi