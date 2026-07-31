// SystemApp - commands/mounts
// systemapp mounts [--json]
// Lists detected system-related partitions and their mount state.
// Full mount *manager* (remount rw/ro, overlayfs/bind/tmpfs fallback chain,
// A/B + dynamic-partition remount engine) is tracked as a follow-up: this
// command is the read-only inspection half of that subsystem.

#include "systemapp/command.hpp"
#include "systemapp/partition_detector.hpp"
#include "systemapp/json.hpp"
#include "systemapp/color.hpp"

#include <iostream>
#include <fstream>

namespace systemapp::commands {

using systemapp::core::CommandContext;
using systemapp::core::ICommand;
using systemapp::core::JsonValue;

class MountsCommand : public ICommand {
public:
    std::string name() const override { return "mounts"; }
    std::string summary() const override { return "Show detected system/vendor/product partitions and mount state"; }
    std::string usage() const override { return "systemapp mounts [--json]"; }

    int run(const CommandContext& ctx) override {
        auto partitions = systemapp::mount::PartitionDetector::detect();

        if (ctx.json_output) {
            auto arr = JsonValue::array();
            for (const auto& p : partitions) {
                auto pj = JsonValue::object();
                pj.set("name", p.name);
                pj.set("mount_point", p.mount_point);
                pj.set("device", p.device);
                pj.set("fs_type", p.fs_type);
                pj.set("options", p.options);
                pj.set("read_only", p.read_only);
                pj.set("overlay", p.is_overlay);
                arr.push_back(pj);
            }
            std::cout << arr.dump() << "\n";
            return 0;
        }

        using namespace systemapp::utils;
        if (partitions.empty()) {
            std::cout << yellow("No known partitions detected in /proc/mounts.\n");
            return 1;
        }
        for (const auto& p : partitions) {
            std::cout << bold(p.name) << "\n";
            std::cout << "  mount  : " << p.mount_point << "\n";
            std::cout << "  device : " << p.device << "\n";
            std::cout << "  fstype : " << p.fs_type << "\n";
            std::cout << "  mode   : " << (p.read_only ? red("ro") : green("rw"))
                       << (p.is_overlay ? " (overlay)" : "") << "\n";
            std::cout << "  opts   : " << p.options << "\n\n";
        }
        return 0;
    }
};

} // namespace systemapp::commands

SYSTEMAPP_REGISTER_COMMAND(systemapp::commands::MountsCommand);
