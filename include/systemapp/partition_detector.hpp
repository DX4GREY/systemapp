#pragma once
// SystemApp - mount/PartitionDetector
// Enumerates well-known Android partitions and their current mount state
// by parsing /proc/mounts and /proc/self/mountinfo. Supports A/B slot
// suffixes and dynamic-partition (super) layouts implicitly, since it
// works purely off what the kernel reports as currently mounted.

#include <string>
#include <vector>

namespace systemapp::mount {

struct PartitionInfo {
    std::string name;         // e.g. "system", "vendor", "product"
    std::string mount_point;  // e.g. "/", "/vendor"
    std::string device;       // backing device/loop as reported by the kernel
    std::string fs_type;      // e.g. "ext4", "erofs", "overlay", "f2fs"
    std::string options;      // raw mount options, e.g. "ro,seclabel,..."
    bool read_only = true;
    bool is_overlay = false;
};

class PartitionDetector {
public:
    // Known logical partitions SystemApp understands. Detection checks for
    // each of these as a mount point and/or block device; absent ones are
    // simply omitted from the result (e.g. odm may not exist on a device).
    static const std::vector<std::string>& known_partitions();

    // Parses /proc/mounts and returns entries matching known partitions.
    static std::vector<PartitionInfo> detect();

    // Returns true if the given mount point currently reports "rw" among its options.
    static bool is_writable(const PartitionInfo& p);
};

} // namespace systemapp::mount
