// SystemApp - entrypoint
// git-style CLI: `systemapp <command> [args...] [flags]`
// Global flags (-h/--help, -v/--version, --json, --verbose, --force,
// --dry-run, --no-reboot) are parsed here and stripped out of the args
// forwarded to each command's CommandContext.

#include "systemapp/command.hpp"
#include "systemapp/logger.hpp"
#include "systemapp/color.hpp"
#include "systemapp/version.hpp"

#include <iostream>
#include <algorithm>
#include <vector>
#include <string>

using systemapp::core::CommandContext;
using systemapp::core::CommandRegistry;
using systemapp::core::Logger;
using systemapp::core::LogLevel;

namespace {

void print_global_help() {
    using namespace systemapp::utils;
    std::cout << bold("systemapp") << " " << systemapp::kVersion
              << " - native Android system administration CLI\n\n";
    std::cout << "USAGE\n  systemapp <command> [args] [flags]\n\n";
    std::cout << "GLOBAL FLAGS\n"
                 "  -h, --help       Show this help (or 'systemapp <command> -h' for command help)\n"
                 "  -v, --version    Show version\n"
                 "      --json       Emit machine-readable JSON output\n"
                 "      --verbose    Verbose logging\n"
                 "      --force      Skip confirmation prompts\n"
                 "      --dry-run    Show what would happen without making changes\n"
                 "      --no-reboot  Skip any reboot step after an operation\n\n";
    std::cout << bold("COMMANDS\n");
    for (const auto& [cmd_name, cmd] : CommandRegistry::instance().all()) {
        std::cout << "  " << cmd_name;
        for (size_t i = cmd_name.size(); i < 14; ++i) std::cout << ' ';
        std::cout << cmd->summary() << "\n";
    }
    std::cout << "\nRun 'systemapp <command> --help' for command-specific usage.\n";
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> raw_args(argv + 1, argv + argc);

    bool want_version = false;
    bool want_help = false;
    CommandContext ctx;

    std::string command_name;
    std::vector<std::string> remaining;

    for (size_t i = 0; i < raw_args.size(); ++i) {
        const std::string& a = raw_args[i];
        if (command_name.empty() && !a.empty() && a[0] != '-') {
            command_name = a;
            continue;
        }
        if (a == "-h" || a == "--help") { want_help = true; }
        else if (a == "-v" || a == "--version") { want_version = true; }
        else if (a == "--json") { ctx.json_output = true; }
        else if (a == "--verbose") { ctx.verbose = true; }
        else if (a == "--force") { ctx.force = true; }
        else if (a == "--dry-run") { ctx.dry_run = true; }
        else if (a == "--no-reboot") { ctx.no_reboot = true; }
        else { remaining.push_back(a); }
    }
    ctx.args = remaining;

    Logger::instance().set_level(ctx.verbose ? LogLevel::VERBOSE : LogLevel::INFO);
    Logger::instance().set_color_enabled(systemapp::utils::color_supported());

    if (want_version && command_name.empty()) {
        std::cout << systemapp::kName << " " << systemapp::kVersion << "\n";
        return 0;
    }

    if (command_name.empty()) {
        print_global_help();
        return 0;
    }

    auto* cmd = CommandRegistry::instance().find(command_name);
    if (!cmd) {
        std::cerr << systemapp::utils::red("error: ") << "unknown command '" << command_name << "'\n";
        std::cerr << "Run 'systemapp --help' to see available commands.\n";
        return 127;
    }

    if (want_help) {
        std::cout << cmd->summary() << "\n\n" << cmd->usage() << "\n";
        return 0;
    }

    try {
        return cmd->run(ctx);
    } catch (const std::exception& e) {
        Logger::instance().error(std::string("unhandled exception: ") + e.what());
        return 1;
    }
}
