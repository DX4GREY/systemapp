// SystemApp - core/RootDetector implementation
#include "systemapp/root_detector.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>

namespace systemapp::core {

namespace {

bool path_exists(const char* p) {
    struct stat st{};
    return ::stat(p, &st) == 0;
}

// Runs a shell command and captures stdout (trimmed). Returns empty string
// on failure. Intentionally uses popen rather than a hand-rolled fork/exec
// since these are trusted, hardcoded diagnostic commands (no user input is
// ever interpolated into the command string).
std::string capture(const char* cmd) {
    std::array<char, 256> buf{};
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) return result;
    while (fgets(buf.data(), buf.size(), pipe.get()) != nullptr) {
        result += buf.data();
    }
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' ')) {
        result.pop_back();
    }
    return result;
}

bool binary_on_path(const char* name) {
    std::string cmd = std::string("command -v ") + name + " 2>/dev/null";
    return !capture(cmd.c_str()).empty();
}

} // namespace

RootInfo RootDetector::detect() {
    RootInfo info;
    info.is_root_uid = (::geteuid() == 0);

    // KernelSU exposes /data/adb/ksu and a ksud/ksu tool, plus a kernel
    // interface at /proc/ksu (or /sys/kernel/kernelsu on some builds).
    if (path_exists("/data/adb/ksu") || path_exists("/proc/ksu") ||
        path_exists("/sys/kernel/kernelsu")) {
        info.provider = RootProvider::KERNELSU;
        info.binary_path = "/data/adb/ksu/bin/ksud";
        info.version = capture("ksud -V 2>/dev/null");
        return info;
    }

    // APatch installs to /data/adb/ap and exposes an "apd" daemon.
    if (path_exists("/data/adb/ap") || binary_on_path("apd")) {
        info.provider = RootProvider::APATCH;
        info.binary_path = "/data/adb/ap/bin/apd";
        info.version = capture("apd -V 2>/dev/null");
        return info;
    }

    // Magisk installs magisk/magiskpolicy binaries and /data/adb/magisk.
    if (path_exists("/data/adb/magisk") || binary_on_path("magisk")) {
        info.provider = RootProvider::MAGISK;
        info.binary_path = "/data/adb/magisk/magisk";
        info.version = capture("magisk -v 2>/dev/null");
        return info;
    }

    // Generic su fallback (e.g. userdebug builds, other root solutions).
    if (binary_on_path("su")) {
        info.provider = RootProvider::SU_GENERIC;
        info.binary_path = capture("command -v su 2>/dev/null");
        info.version = capture("su -v 2>/dev/null");
        return info;
    }

    info.provider = RootProvider::NONE;
    return info;
}

std::string RootDetector::provider_name(RootProvider p) {
    switch (p) {
        case RootProvider::MAGISK: return "Magisk";
        case RootProvider::KERNELSU: return "KernelSU";
        case RootProvider::APATCH: return "APatch";
        case RootProvider::SU_GENERIC: return "su (generic)";
        default: return "none";
    }
}

} // namespace systemapp::core
