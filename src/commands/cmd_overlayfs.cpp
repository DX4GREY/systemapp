// SystemApp - commands/overlayfs
// systemapp overlayfs <apk_path> [--name=<module_name>]
// Creates a Magisk module directly from the program and installs it to
// /data/adb/modules for dynamic partition devices. The overlay module will
// place the APK in /system/priv-app/<name>/ so it becomes a system app.
// On subsequent runs, if the module is detected, prints "overlayfs is active".

#include "systemapp/command.hpp"
#include "systemapp/logger.hpp"
#include "systemapp/color.hpp"
#include "systemapp/version.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

namespace systemapp::commands {

using systemapp::core::CommandContext;
using systemapp::core::ICommand;
using namespace systemapp::utils;

namespace {

bool dir_exists(const std::string& path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool file_exists(const std::string& path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) return {};
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool write_file(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (!f) return false;
    f << content;
    return true;
}

std::string get_module_dir(const std::string& module_name) {
    return "/data/adb/modules/" + module_name;
}

bool is_overlay_module_installed(const std::string& module_name) {
    std::string module_dir = get_module_dir(module_name);
    std::string module_prop = module_dir + "/module.prop";
    if (!file_exists(module_prop)) return false;
    std::string content = read_file(module_prop);
    return content.find("id=" + module_name) != std::string::npos;
}

} // namespace

class OverlayfsCommand : public ICommand {
public:
    std::string name() const override { return "overlayfs"; }
    std::string summary() const override { return "Create and install an OverlayFS Magisk module for system apps"; }
    std::string usage() const override { return "systemapp overlayfs <apk_path> [--name=<module_name>]"; }

    int run(const CommandContext& ctx) override {
        if (ctx.args.empty()) {
            std::cerr << red("error: ") << "APK path required\n";
            std::cerr << "Usage: " << usage() << "\n";
            return 1;
        }

        std::string apk_path = ctx.args[0];
        std::string module_name = "systemapp-overlay";

        // Parse --name flag
        for (size_t i = 1; i < ctx.args.size(); ++i) {
            if (ctx.args[i].find("--name=") == 0) {
                module_name = ctx.args[i].substr(7);
            }
        }

        if (!file_exists(apk_path)) {
            std::cerr << red("error: ") << "APK not found: " << apk_path << "\n";
            return 1;
        }

        // Check if module already installed
        if (is_overlay_module_installed(module_name)) {
            std::cout << green("overlayfs is active") << "\n";
            std::cout << "Module: " << module_name << "\n";
            std::cout << "Path: " << get_module_dir(module_name) << "\n";
            return 0;
        }

        // Check root
        if (getuid() != 0) {
            std::cerr << red("error: ") << "root access required to install Magisk module\n";
            return 1;
        }

        // Setup module directories
        std::string module_dir = get_module_dir(module_name);
        std::string priv_app_dir = module_dir + "/system/priv-app/" + module_name;
        std::string upper_dir = module_dir + "/overlay/upper/system/priv-app/" + module_name;
        std::string work_dir = module_dir + "/overlay/work";

        std::string cmds = "mkdir -p " + priv_app_dir + " " + upper_dir + " " + work_dir + " && ";
        cmds += "cp " + apk_path + " " + upper_dir + "/" + module_name + ".apk && ";
        cmds += "chmod 644 " + upper_dir + "/" + module_name + ".apk && ";
        cmds += "cp " + apk_path + " " + priv_app_dir + "/" + module_name + ".apk && ";
        cmds += "chmod 644 " + priv_app_dir + "/" + module_name + ".apk";

        int ret = system(cmds.c_str());
        if (ret != 0) {
            std::cerr << red("error: ") << "Failed to copy APK to module directories\n";
            return 1;
        }

        // Create module.prop
        std::string module_prop =
            "id=" + module_name + "\n"
            "name=SystemApp Overlay (" + module_name + ")\n"
            "version=v" + std::string(systemapp::kVersion) + "\n"
            "versionCode=" + std::to_string(atoi(systemapp::kVersion) * 10000 + 1) + "\n"
            "author=SystemApp Project\n"
            "description=OverlayFS Magisk module for " + module_name + " system app.\n";

        if (!write_file(module_dir + "/module.prop", module_prop)) {
            std::cerr << red("error: ") << "Failed to write module.prop\n";
            return 1;
        }

        // Create post-fs-data.sh
        std::string post_fs =
            "#!/system/bin/sh\n"
            "MODULE_OVERLAY=\"$MODPATH/overlay\"\n"
            "UPPER=\"$MODULE_OVERLAY/upper\"\n"
            "WORK=\"$MODULE_OVERLAY/work\"\n"
            "SYSTEM_MOUNT=\"/system\"\n"
            "\n"
            "mkdir -p \"$UPPER/system\" \"$WORK\"\n"
            "\n"
            "if ! mount | grep -q \"${SYSTEM_MOUNT}.*overlay\"; then\n"
            "    mount -t overlay overlay -o lowerdir=${SYSTEM_MOUNT},upperdir=${UPPER}/system,workdir=${WORK} ${SYSTEM_MOUNT} || exit 1\n"
            "fi\n";

        if (!write_file(module_dir + "/post-fs-data.sh", post_fs)) {
            std::cerr << red("error: ") << "Failed to write post-fs-data.sh\n";
            return 1;
        }

        // Create service.sh
        std::string service =
            "#!/system/bin/sh\n"
            "SYSTEM_MOUNT=\"/system\"\n"
            "umount \"${SYSTEM_MOUNT}\" 2>/dev/null || true\n";

        if (!write_file(module_dir + "/service.sh", service)) {
            std::cerr << red("error: ") << "Failed to write service.sh\n";
            return 1;
        }

        // Set permissions
        system(("chmod 755 " + module_dir + "/post-fs-data.sh " + module_dir + "/service.sh").c_str());
        system(("chmod -R 755 " + module_dir + "/overlay").c_str());

        std::cout << green("OverlayFS module created successfully") << "\n";
        std::cout << "Module path: " << module_dir << "\n";
        std::cout << "Module name: " << module_name << "\n";
        std::cout << "Please reboot for the overlay to take effect.\n";

        return 0;
    }
};

} // namespace systemapp::commands

SYSTEMAPP_REGISTER_COMMAND(systemapp::commands::OverlayfsCommand);