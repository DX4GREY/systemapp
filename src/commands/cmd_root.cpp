// SystemApp - commands/root
// systemapp root [--json]
// Standalone root-detection command (also used internally by other
// commands to abort dangerous operations without root).

#include "systemapp/command.hpp"
#include "systemapp/root_detector.hpp"
#include "systemapp/json.hpp"
#include "systemapp/color.hpp"

#include <iostream>

namespace systemapp::commands {

using systemapp::core::CommandContext;
using systemapp::core::ICommand;
using systemapp::core::JsonValue;
using systemapp::core::RootDetector;

class RootCommand : public ICommand {
public:
    std::string name() const override { return "root"; }
    std::string summary() const override { return "Detect available root provider (su/Magisk/KernelSU/APatch)"; }
    std::string usage() const override { return "systemapp root [--json]"; }

    int run(const CommandContext& ctx) override {
        auto info = RootDetector::detect();
        bool has_root = RootDetector::has_root(info);
        std::string provider = RootDetector::provider_name(info.provider);

        if (ctx.json_output) {
            auto root = JsonValue::object();
            root.set("has_root", has_root);
            root.set("provider", provider);
            root.set("is_root_uid", info.is_root_uid);
            root.set("binary_path", info.binary_path);
            root.set("version", info.version);
            std::cout << root.dump() << "\n";
        } else {
            using namespace systemapp::utils;
            std::cout << "Provider: " << (has_root ? green(provider) : red(provider)) << "\n";
            std::cout << "UID 0   : " << (info.is_root_uid ? "yes" : "no") << "\n";
            if (!info.binary_path.empty()) std::cout << "Binary  : " << info.binary_path << "\n";
            if (!info.version.empty()) std::cout << "Version : " << info.version << "\n";
        }
        // Exit code doubles as a scriptable check: 0 = root available.
        return has_root ? 0 : 1;
    }
};

} // namespace systemapp::commands

SYSTEMAPP_REGISTER_COMMAND(systemapp::commands::RootCommand);
