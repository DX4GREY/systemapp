#!/usr/bin/env bash
# Build a Termux-compatible .deb package around the already-built arm64-v8a
# binary. Does NOT recompile from source, since doing that on a non-Android
# host would silently package a host-architecture binary labeled "aarch64"
# (that was a bug in an earlier version of this script - packaging must
# reuse the actual cross-compiled Android binary, not a host rebuild).
set -euo pipefail
cd "$(dirname "$0")/.."

VERSION="$(grep -oP '(?<=kVersion = ")[^"]+' include/systemapp/version.hpp)"
BIN_SRC="release/systemapp-arm64-v8a"
PKG_ROOT="build-termux/pkgroot"
TERMUX_PREFIX="${PREFIX:-/data/data/com.termux/files/usr}"
BIN_DIR="${PKG_ROOT}${TERMUX_PREFIX}/bin"

if [[ ! -f "${BIN_SRC}" ]]; then
    echo "error: ${BIN_SRC} not found - run 'ABI=arm64-v8a ./build.sh binary' first" >&2
    exit 1
fi

rm -rf build-termux
mkdir -p "${BIN_DIR}" "${PKG_ROOT}/DEBIAN"

cp "${BIN_SRC}" "${BIN_DIR}/systemapp"
chmod 755 "${BIN_DIR}/systemapp"

cat > "${PKG_ROOT}/DEBIAN/control" <<EOF
Package: systemapp
Version: ${VERSION}
Architecture: aarch64
Maintainer: SystemApp Project
Description: Native Android system administration CLI (root-only operations)
 SystemApp is a BusyBox/Toybox-style native CLI for managing system apps,
 partitions, properties, and root-level Android system state.
EOF

mkdir -p release
dpkg-deb --build "${PKG_ROOT}" "release/systemapp.deb"
echo "Built release/systemapp.deb"
