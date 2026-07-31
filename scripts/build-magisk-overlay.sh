#!/usr/bin/env bash
# Package systemapp as an overlayfs Magisk module for dynamic partition devices.
# This module does not modify the actual system partition; it mounts an overlay
# that adds/updates files in /system. Works on devices with dynamic partitions
# where system is read-only and cannot be remounted rw.
set -euo pipefail
cd "$(dirname "$0")/.."

ABI="${ABI:-arm64-v8a}"
VERSION="$(grep -oP '(?<=kVersion = ")[^"]+' include/systemapp/version.hpp)"
STAGE="build-magisk-overlay/module-${ABI}"
BIN_SRC="release/systemapp-${ABI}"

if [[ ! -f "${BIN_SRC}" ]]; then
    echo "error: ${BIN_SRC} not found - run 'ABI=${ABI} ./build.sh binary' first" >&2
    exit 1
fi

rm -rf "${STAGE}"
mkdir -p "${STAGE}/system/bin"
mkdir -p "${STAGE}/overlay/bin"
mkdir -p "${STAGE}/overlay/work"
mkdir -p "${STAGE}/overlay/upper/system/bin"

# Copy binary to overlay upper dir (this is what will appear in /system/bin)
cp "${BIN_SRC}" "${STAGE}/overlay/upper/system/bin/systemapp"
chmod 755 "${STAGE}/overlay/upper/system/bin/systemapp"

# Script to mount overlayfs on /system during boot
cat > "${STAGE}/post-fs-data.sh" <<'EOF'
#!/system/bin/sh
# SystemApp OverlayFS Module - Mount overlay on /system to add systemapp binary
# without modifying the actual system partition. For dynamic partition devices.

# Wait for system to be ready
sleep 2

# Find the actual system mount point
SYSTEM_MOUNT=""
if [[ -d /system_ext/etc ]]; then
    # Some devices mount system as /system_ext
    SYSTEM_MOUNT="/system_ext"
elif [[ -d /system/etc ]]; then
    SYSTEM_MOUNT="/system"
fi

if [[ -z "${SYSTEM_MOUNT}" ]]; then
    echo "[systemapp] Could not find system mount point"
    exit 1
fi

# Overlay directories inside module path
MODULE_OVERLAY="${MODPATH}/overlay"
UPPER_DIR="${MODULE_OVERLAY}/upper"
WORK_DIR="${MODULE_OVERLAY}/work"

# Ensure directories exist
mkdir -p "${UPPER_DIR}/system" "${WORK_DIR}"

# Check if already mounted
if mount | grep -q "${SYSTEM_MOUNT}.*overlay"; then
    echo "[systemapp] Overlay already mounted on ${SYSTEM_MOUNT}"
    exit 0
fi

# Mount overlayfs
mount -t overlay overlay -o lowerdir=${SYSTEM_MOUNT},upperdir=${UPPER_DIR}/system,workdir=${WORK_DIR} ${SYSTEM_MOUNT} || {
    echo "[systemapp] Failed to mount overlay on ${SYSTEM_MOUNT}"
    exit 1
}

echo "[systemapp] Overlay mounted on ${SYSTEM_MOUNT}"

# Ensure permissions are correct
chmod 755 "${SYSTEM_MOUNT}/bin/systemapp" 2>/dev/null || true
EOF
chmod 755 "${STAGE}/post-fs-data.sh"

# Script to unmount overlay on module uninstall/disable
cat > "${STAGE}/service.sh" <<'EOF'
#!/system/bin/sh
# SystemApp OverlayFS Module - Cleanup

# Find system mount point
SYSTEM_MOUNT=""
if [[ -d /system_ext/etc ]]; then
    SYSTEM_MOUNT="/system_ext"
elif [[ -d /system/etc ]]; then
    SYSTEM_MOUNT="/system"
fi

if [[ -n "${SYSTEM_MOUNT}" ]]; then
    umount "${SYSTEM_MOUNT}" 2>/dev/null || true
fi
EOF
chmod 755 "${STAGE}/service.sh"

# Module properties
cat > "${STAGE}/module.prop" <<EOF
id=systemapp-overlay
name=SystemApp OverlayFS
version=v${VERSION}
versionCode=$(date +%Y%m%d)
author=SystemApp Project
description=OverlayFS module to install systemapp binary on dynamic partition devices without modifying system partition.
EOF

# For Magisk modules, we also need to place the binary in the overlay dir
# that gets mounted. The main binary for non-overlay installation stays in
# an optional system/bin for legacy devices.
cat > "${STAGE}/system.prop" <<'EOF'
ro.systemapp.overlayfs=true
EOF

mkdir -p release
( cd "${STAGE}" && zip -r -X "../../release/SystemApp-Overlay-Magisk-${ABI}.zip" . >/dev/null )
echo "Built release/SystemApp-Overlay-Magisk-${ABI}.zip"