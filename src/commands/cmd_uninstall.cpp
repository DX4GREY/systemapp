// SystemApp - commands/uninstall
// systemapp uninstall <app_name>
// Uninstalls a system app by removing it from overlayfs overlay.
// Requires the overlayfs module to be active.

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
#include <dirent.h>

namespace systemapp::commands {

using systemapp::core::CommandContext;
using systemapp::core::ICommand;
using namespace systemapp::utils;

namespace {

bool dir_exists(const std::string& path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::string detect_overlay_module() {
    // Look for any module with overlayfs active in /data/adb/modules
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
                return module_path;
            }
        }
    }
    closedir(dir);
    return {};
}

} // namespace

class UninstallCommand : public ICommand {
public:
    std::string name() const override { return "uninstall"; }
    std::string summary() const override { return "Uninstall a system app from overlayfs"; }
    std::string usage() const override { return "systemapp uninstall <app_name>"; }

    int run(const CommandContext& ctx) override {
        if (ctx.args.empty()) {
            std::cerr << red("error: ") << "App name required\n";
            std::cerr << "Usage: " << usage() << "\n";
            return 1;
        }

        std::string app_name = ctx.args[0];

        // Detect overlay module
        std::string module_path = detect_overlay_module();
        if (module_path.empty()) {
            std::cerr << red("error: ") << "No active overlayfs module found.\n";
            std::cerr << "Run 'systemapp overlayfs <apk_path>' first to create a module.\n";
            return 1;
        }

        // Check root
        if (getuid() != 0) {
            std::cerr << red("error: ") << "root access required\n";
            return 1;
        }

        // Remove APK from overlay upper dir
        std::string overlay_upper = module_path + "/overlay/upper/system/priv-app/" + app_name;
        std::string dest_apk = overlay_upper + "/" + app_name + ".apk";

        // Check if APK exists
        struct stat st{};
        if (stat(dest_apk.c_str(), &st) != 0) {
            std::cerr << yellow("warning: ") << "App '" << app_name << "' not found in overlay.\n";
            return 1;
        }

        // Remove APK and directory
        std::string cmd = "rm -f '" + dest_apk + "' && ";
        cmd += "rmdir '" + overlay_upper + "' 2>/dev/null || true";
        cmd += " && rmdir '" + module_path + "/overlay/upper/system/priv-app/" + app_name + "' 2>/dev/null || true";

        int ret = system(cmd.c_str());
        if (ret != 0) {
            std::cerr << red("error: ") << "Failed to uninstall APK\n";
            return 1;
        }

        std::cout << green("System app uninstalled successfully") << "\n";
        std::cout << "App name: " << app_name << "\n";
        std::cout << "Overlay module: " << module_path << "\n";
        std::cout << "Please reboot for changes to take effect.\n";

        return 0;
    }
};

} // namespace systemapp::commands

SYSTEMAPP_REGISTER_COMMAND(systemapp::commands::UninstallCommand);