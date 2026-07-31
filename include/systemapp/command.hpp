#pragma once
// SystemApp - core/Command + CommandRegistry
// Every subcommand (install, uninstall, list, remount, ...) implements
// ICommand and self-registers via REGISTER_COMMAND. main.cpp never needs to
// know about individual commands, so adding a new one is a single new .cpp
// file under src/commands/ with no changes to core code.

#include <string>
#include <vector>
#include <functional>
#include <map>
#include <memory>

namespace systemapp::core {

struct CommandContext {
    std::vector<std::string> args;   // positional args after the subcommand name
    bool json_output = false;
    bool verbose = false;
    bool force = false;
    bool dry_run = false;
    bool no_reboot = false;
};

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual std::string name() const = 0;
    virtual std::string summary() const = 0;
    virtual std::string usage() const = 0;
    // Return process exit code (0 == success).
    virtual int run(const CommandContext& ctx) = 0;
};

class CommandRegistry {
public:
    static CommandRegistry& instance() {
        static CommandRegistry inst;
        return inst;
    }

    void register_command(std::shared_ptr<ICommand> cmd) {
        commands_[cmd->name()] = std::move(cmd);
    }

    ICommand* find(const std::string& name) const {
        auto it = commands_.find(name);
        return it == commands_.end() ? nullptr : it->second.get();
    }

    const std::map<std::string, std::shared_ptr<ICommand>>& all() const { return commands_; }

private:
    std::map<std::string, std::shared_ptr<ICommand>> commands_;
};

// Registers a command instance at static-init time.
struct CommandRegistrar {
    explicit CommandRegistrar(const std::shared_ptr<ICommand>& cmd) {
        CommandRegistry::instance().register_command(cmd);
    }
};

#define SYSTEMAPP_CONCAT_INNER(a, b) a##b
#define SYSTEMAPP_CONCAT(a, b) SYSTEMAPP_CONCAT_INNER(a, b)

// ClassName may be namespace-qualified (e.g. systemapp::commands::InfoCommand),
// so the generated variable name is disambiguated with __COUNTER__ rather
// than by pasting the (unpasteable, "::"-containing) class name itself.
#define SYSTEMAPP_REGISTER_COMMAND(ClassName) \
    static systemapp::core::CommandRegistrar SYSTEMAPP_CONCAT(_systemapp_registrar_, __COUNTER__)( \
        std::make_shared<ClassName>())

} // namespace systemapp::core
