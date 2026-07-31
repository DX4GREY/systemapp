#!/usr/bin/env bash
# Package systemapp as a flashable Magisk module zip, one per ABI.
# Requires scripts/build-binary.sh to have already produced
# release/systemapp-<ABI> (or run this after ./build.sh binary with the
# matching ABI env var set).
set -euo pipefail
cd "$(dirname "$0")/.."

ABI="${ABI:-arm64-v8a}"
VERSION="$(grep -oP '(?<=kVersion = ")[^"]+' include/systemapp/version.hpp)"
STAGE="build-magisk/module-${ABI}"
BIN_SRC="release/systemapp-${ABI}"

if [[ ! -f "${BIN_SRC}" ]]; then
    echo "error: ${BIN_SRC} not found - run 'ABI=${ABI} ./build.sh binary' first" >&2
    exit 1
fi

rm -rf "${STAGE}"
mkdir -p "${STAGE}/system/bin"
cp "${BIN_SRC}" "${STAGE}/system/bin/systemapp"
chmod 755 "${STAGE}/system/bin/systemapp"

cat > "${STAGE}/module.prop" <<EOF
id=systemapp
name=SystemApp
version=v${VERSION}
versionCode=$(date +%Y%m%d)
author=SystemApp Project
description=Native Android system administration CLI (install/uninstall system apps, remount, debloat, props, mounts). Built for ${ABI}.
EOF

cat > "${STAGE}/customize.sh" <<'EOF'
# Runs during Magisk module install. Nothing device-specific needed since
# the binary is statically linked; this just ensures the exec bit survives
# packaging on filesystems that don't preserve it.
set_perm_recursive "$MODPATH/system/bin" 0 0 0755 0755
EOF

touch "${STAGE}/skip_mount"  # module ships a binary only, no need to mount system/

mkdir -p release
( cd "${STAGE}" && zip -r -X "../../release/SystemApp-Magisk-${ABI}.zip" . >/dev/null )
echo "Built release/SystemApp-Magisk-${ABI}.zip"
