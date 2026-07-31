// SystemApp - commands/install
// systemapp install <apk_path> [--name=<app_name>]
// Installs a system app by copying APK to /system/priv-app/<name>/ via overlayfs.
// Requires the overlayfs module to be active (created via `systemapp overlayfs`).

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

bool file_exists(const std::string& path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
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
        std::string module_prop = module_path + "/module.prop";
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

class InstallCommand : public ICommand {
public:
    std::string name() const override { return "install"; }
    std::string summary() const override { return "Install a system app APK via overlayfs"; }
    std::string usage() const override { return "systemapp install <apk_path> [--name=<app_name>]"; }

    int run(const CommandContext& ctx) override {
        if (ctx.args.empty()) {
            std::cerr << red("error: ") << "APK path required\n";
            std::cerr << "Usage: " << usage() << "\n";
            return 1;
        }

        std::string apk_path = ctx.args[0];
        std::string app_name;

        // Parse --name flag
        for (size_t i = 1; i < ctx.args.size(); ++i) {
            if (ctx.args[i].find("--name=") == 0) {
                app_name = ctx.args[i].substr(7);
            }
        }

        if (!file_exists(apk_path)) {
            std::cerr << red("error: ") << "APK not found: " << apk_path << "\n";
            return 1;
        }

        // Detect overlay module
        std::string module_path = detect_overlay_module();
        if (module_path.empty()) {
            std::cerr << red("error: ") << "No active overlayfs module found.\n";
            std::cerr << "Run 'systemapp overlayfs <apk_path>' first to create a module.\n";
            return 1;
        }

        // Determine app name from APK filename if not provided
        if (app_name.empty()) {
            size_t pos = apk_path.find_last_of("/\\");
            std::string filename = (pos == std::string::npos) ? apk_path : apk_path.substr(pos + 1);
            size_t dot_pos = filename.find_last_of('.');
            app_name = (dot_pos == std::string::npos) ? filename : filename.substr(0, dot_pos);
        }

        // Check root
        if (getuid() != 0) {
            std::cerr << red("error: ") << "root access required\n";
            return 1;
        }

        // Copy APK to overlay upper dir
        std::string overlay_upper = module_path + "/overlay/upper/system/priv-app/" + app_name;
        std::string dest_apk = overlay_upper + "/" + app_name + ".apk";

        std::string cmd = "mkdir -p " + overlay_upper + " && ";
        cmd += "cp '" + apk_path + "' '" + dest_apk + "' && ";
        cmd += "chmod 644 '" + dest_apk + "'";

        int ret = system(cmd.c_str());
        if (ret != 0) {
            std::cerr << red("error: ") << "Failed to install APK\n";
            return 1;
        }

        std::cout << green("System app installed successfully") << "\n";
        std::cout << "App name: " << app_name << "\n";
        std::cout << "APK path: /system/priv-app/" << app_name << "/" << app_name << ".apk\n";
        std::cout << "Overlay module: " << module_path << "\n";
        std::cout << "Please reboot for changes to take effect.\n";

        return 0;
    }
};

} // namespace systemapp::commands

SYSTEMAPP_REGISTER_COMMAND(systemapp::commands::InstallCommand);