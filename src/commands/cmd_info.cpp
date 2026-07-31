// SystemApp - commands/info
// systemapp info [--json]
// Prints device/build/root summary. Good smoke-test command since it
// touches RootDetector, PartitionDetector, and both output modes.

#include "systemapp/command.hpp"
#include "systemapp/root_detector.hpp"
#include "systemapp/partition_detector.hpp"
#include "systemapp/json.hpp"
#include "systemapp/color.hpp"
#include "systemapp/version.hpp"

#include <iostream>
#include <sys/utsname.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <dirent.h>

namespace systemapp::commands {

using systemapp::core::CommandContext;
using systemapp::core::ICommand;
using systemapp::core::JsonValue;

namespace {

std::string read_prop(const std::string& name) {
    // Minimal getprop reader: shells out to the system getprop binary since
    // parsing the Android property service's binary trie natively is a
    // larger piece of work tracked separately (see PropsCommand TODOs).
    std::string cmd = "getprop " + name + " 2>/dev/null";
    std::array<char, 256> buf{};
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return result;
    while (fgets(buf.data(), buf.size(), pipe.get())) result += buf.data();
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) result.pop_back();
    return result;
}

std::string detect_overlay_module_name() {
    std::string modules_dir = "/data/adb/modules";
    struct stat st{};
    if (stat(modules_dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        return {};
    }

    DIR* dir = opendir(modules_dir.c_str());
    if (!dir) return {};

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        
        std::string module_path = modules_dir + "/" + std::string(entry->d_name);
        std::string overlay_dir = module_path + "/overlay/upper/system";
        
        struct stat module_st{};
        if (stat(module_path.c_str(), &module_st) == 0 && S_ISDIR(module_st.st_mode)) {
            struct stat overlay_st{};
            if (stat(overlay_dir.c_str(), &overlay_st) == 0 && S_ISDIR(overlay_st.st_mode)) {
                closedir(dir);
                return std::string(entry->d_name);
            }
        }
    }
    closedir(dir);
    return {};
}

} // namespace

class InfoCommand : public ICommand {
public:
    std::string name() const override { return "info"; }
    std::string summary() const override { return "Show device, build, and root summary"; }
    std::string usage() const override { return "systemapp info [--json]"; }

    int run(const CommandContext& ctx) override {
        struct utsname uts{};
        uname(&uts);

        auto root_info = systemapp::core::RootDetector::detect();
        auto partitions = systemapp::mount::PartitionDetector::detect();

        std::string manufacturer = read_prop("ro.product.manufacturer");
        std::string model = read_prop("ro.product.model");
        std::string android_ver = read_prop("ro.build.version.release");
        std::string sdk = read_prop("ro.build.version.sdk");
        std::string abi = read_prop("ro.product.cpu.abi");
        std::string fingerprint = read_prop("ro.build.fingerprint");

        if (ctx.json_output) {
            auto root = JsonValue::object();
            root.set("systemapp_version", systemapp::kVersion);
            root.set("kernel", std::string(uts.release));
            root.set("arch", std::string(uts.machine));
            root.set("manufacturer", manufacturer);
            root.set("model", model);
            root.set("android_version", android_ver);
            root.set("sdk", sdk);
            root.set("abi", abi);
            root.set("fingerprint", fingerprint);

            auto root_obj = JsonValue::object();
            root_obj.set("provider", systemapp::core::RootDetector::provider_name(root_info.provider));
            root_obj.set("has_root", systemapp::core::RootDetector::has_root(root_info));
            root_obj.set("is_root_uid", root_info.is_root_uid);
            root_obj.set("binary_path", root_info.binary_path);
            root.set("root", root_obj);

            auto parts = JsonValue::array();
            for (const auto& p : partitions) {
                auto pj = JsonValue::object();
                pj.set("name", p.name);
                pj.set("mount_point", p.mount_point);
                pj.set("fs_type", p.fs_type);
                pj.set("read_only", p.read_only);
                pj.set("overlay", p.is_overlay);
                parts.push_back(pj);
            }
            root.set("partitions", parts);

            std::string overlay = detect_overlay_module_name();
            root.set("overlayfs_active", !overlay.empty());
            if (!overlay.empty()) root.set("overlay_module", overlay);

            std::cout << root.dump() << "\n";
            return 0;
        }

        using namespace systemapp::utils;
        std::cout << bold("SystemApp ") << systemapp::kVersion << "\n\n";
        std::cout << bold("Device\n");
        std::cout << "  Manufacturer : " << manufacturer << "\n";
        std::cout << "  Model        : " << model << "\n";
        std::cout << "  Android      : " << android_ver << " (SDK " << sdk << ")\n";
        std::cout << "  ABI          : " << abi << "\n";
        std::cout << "  Kernel       : " << uts.release << " (" << uts.machine << ")\n";
        if (!fingerprint.empty()) std::cout << "  Fingerprint  : " << fingerprint << "\n";

        std::cout << "\n" << bold("Root\n");
        std::string provider = systemapp::core::RootDetector::provider_name(root_info.provider);
        bool has_root = systemapp::core::RootDetector::has_root(root_info);
        std::cout << "  Provider     : " << (has_root ? green(provider) : red(provider)) << "\n";
        if (!root_info.version.empty()) std::cout << "  Version      : " << root_info.version << "\n";
        std::cout << "  Effective UID: " << (root_info.is_root_uid ? green("root (0)") : yellow("unprivileged")) << "\n";

        std::cout << "\n" << bold("Partitions\n");
        for (const auto& p : partitions) {
            std::cout << "  " << p.name << " -> " << p.mount_point
                       << " [" << p.fs_type << ", "
                       << (p.read_only ? red("ro") : green("rw"))
                       << (p.is_overlay ? ", overlay" : "") << "]\n";
        }

        std::string overlay = detect_overlay_module_name();
        if (!overlay.empty()) {
            std::cout << "\n" << bold("OverlayFS\n");
            std::cout << "  Status       : " << green("active") << "\n";
            std::cout << "  Module       : " << overlay << "\n";
            std::cout << "  Module path  : /data/adb/modules/" << overlay << "\n";
        } else {
            std::cout << "\n" << bold("OverlayFS\n");
            std::cout << "  Status       : " << red("inactive") << "\n";
        }
        return 0;
    }
};

} // namespace systemapp::commands

SYSTEMAPP_REGISTER_COMMAND(systemapp::commands::InfoCommand);
