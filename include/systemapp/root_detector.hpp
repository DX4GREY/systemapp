#pragma once
// SystemApp - core/RootDetector
// Detects the root provider available on the device: plain su, Magisk,
// KernelSU, or APatch. Used to gate dangerous operations and to select
// the correct resetprop / module-install backend.

#include <string>

namespace systemapp::core {

enum class RootProvider {
    NONE,
    SU_GENERIC,
    MAGISK,
    KERNELSU,
    APATCH
};

struct RootInfo {
    RootProvider provider = RootProvider::NONE;
    std::string version;      // provider version string, if discoverable
    std::string binary_path;  // path to su / provider binary
    bool is_root_uid = false; // true if the current process is already uid 0
};

class RootDetector {
public:
    // Runs full detection. Cheap-ish (a handful of stat/exec calls), safe to
    // call once at startup and cache the result.
    static RootInfo detect();

    // Convenience: true if any usable root provider was found.
    static bool has_root(const RootInfo& info) { return info.provider != RootProvider::NONE; }

    static std::string provider_name(RootProvider p);
};

} // namespace systemapp::core
