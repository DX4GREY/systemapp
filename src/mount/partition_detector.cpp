// SystemApp - mount/PartitionDetector implementation
#include "systemapp/partition_detector.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>

namespace systemapp::mount {

const std::vector<std::string>& PartitionDetector::known_partitions() {
    static const std::vector<std::string> names = {
        "system", "system_ext", "vendor", "product",
        "odm", "vendor_dlkm", "system_dlkm", "odm_dlkm"
    };
    return names;
}

namespace {

// Maps a /proc/mounts mount point to one of our known logical partition
// names. Handles the common "/" == system-as-root case on modern devices.
std::string classify_mount_point(const std::string& mp) {
    static const auto& names = PartitionDetector::known_partitions();
    for (const auto& n : names) {
        if (mp == "/" + n) return n;
    }
    if (mp == "/") return "system"; // system-as-root layout
    return "";
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) out.push_back(item);
    return out;
}

} // namespace

std::vector<PartitionInfo> PartitionDetector::detect() {
    std::vector<PartitionInfo> result;
    std::ifstream mounts("/proc/mounts");
    if (!mounts.is_open()) return result;

    std::string line;
    while (std::getline(mounts, line)) {
        // Format: device mountpoint fstype options dump pass
        auto fields = split(line, ' ');
        if (fields.size() < 4) continue;

        const std::string& device = fields[0];
        const std::string& mount_point = fields[1];
        const std::string& fs_type = fields[2];
        const std::string& options = fields[3];

        std::string logical = classify_mount_point(mount_point);
        if (logical.empty()) continue;

        PartitionInfo info;
        info.name = logical;
        info.mount_point = mount_point;
        info.device = device;
        info.fs_type = fs_type;
        info.options = options;
        info.is_overlay = (fs_type == "overlay");
        info.read_only = (options.find("rw") == std::string::npos);

        // Prefer the last matching entry (overlay mounts stack on top of the
        // base mount and appear later in /proc/mounts), so overwrite rather
        // than skip duplicates.
        auto it = std::find_if(result.begin(), result.end(),
            [&](const PartitionInfo& p) { return p.name == logical; });
        if (it != result.end()) {
            *it = info;
        } else {
            result.push_back(info);
        }
    }
    return result;
}

bool PartitionDetector::is_writable(const PartitionInfo& p) {
    return !p.read_only;
}

} // namespace systemapp::mount
