#!/usr/bin/env bash
# Top-level dispatcher: ./build.sh [binary|termux|magisk|host]
set -euo pipefail
cd "$(dirname "$0")"

TARGET="${1:-binary}"

case "$TARGET" in
    binary) exec scripts/build-binary.sh ;;
    termux) exec scripts/build-termux.sh ;;
    magisk) exec scripts/build-magisk.sh ;;
    overlay) exec scripts/build-magisk-overlay.sh ;;
    host)
        # Host-only debug build (for iterating on non-Android-specific logic
        # in an x86_64 dev container). Not a shipping artifact.
        cmake -B build-host -DCMAKE_BUILD_TYPE=Debug
        cmake --build build-host -j"$(nproc)"
        echo "Built build-host/systemapp (host arch, debug)"
        ;;
    *)
        echo "usage: ./build.sh [binary|termux|magisk|overlay|host]" >&2
        exit 1
        ;;
esac
