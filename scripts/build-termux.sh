#!/usr/bin/env bash
# Build a Termux-compatible .deb package.
# Must be run inside Termux (or a Termux-like prefix) since it links
# against $PREFIX and stages files under Termux's /data/data/com.termux/...
# layout. Produces release/systemapp.deb.
set -euo pipefail
cd "$(dirname "$0")/.."

VERSION="$(grep -oP '(?<=kVersion = ")[^"]+' include/systemapp/version.hpp)"
PKG_ROOT="build-termux/pkgroot"
BIN_DIR="${PKG_ROOT}${PREFIX:-/data/data/com.termux/files/usr}/bin"

rm -rf build-termux
mkdir -p "${BIN_DIR}" "${PKG_ROOT}/DEBIAN"

cmake -B build-termux/src -DCMAKE_BUILD_TYPE=Release
cmake --build build-termux/src -j"$(nproc)"
cp build-termux/src/systemapp "${BIN_DIR}/systemapp"
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
