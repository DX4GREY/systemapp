// SystemApp - commands/props
// systemapp props [search-term] [--json]
// Lists (or greps) Android system properties. Currently shells out to the
// platform getprop binary for enumeration since it already handles the
// property-service trie/socket protocol correctly; a native reader plus
// resetprop-compatible *writer* (for Magisk persistent prop overrides) is
// tracked as a follow-up (see README roadmap).

#include "systemapp/command.hpp"
#include "systemapp/json.hpp"
#include "systemapp/color.hpp"

#include <array>
#include <cstdio>
#include <iostream>
#include <memory>
#include <sstream>
#include <algorithm>

namespace systemapp::commands {

using systemapp::core::CommandContext;
using systemapp::core::ICommand;
using systemapp::core::JsonValue;

namespace {

std::string shell_capture(const std::string& cmd) {
    std::array<char, 512> buf{};
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return result;
    while (fgets(buf.data(), buf.size(), pipe.get()) != nullptr) result += buf.data();
    return result;
}

// getprop -Z style lines look like: [ro.build.version.release]: [14]
struct PropPair { std::string key, value; };

std::vector<PropPair> parse_getprop_output(const std::string& raw) {
    std::vector<PropPair> out;
    std::istringstream iss(raw);
    std::string line;
    while (std::getline(iss, line)) {
        auto first_close = line.find("]: [");
        if (line.empty() || line.front() != '[' || first_close == std::string::npos) continue;
        std::string key = line.substr(1, first_close - 1);
        std::string value = line.substr(first_close + 4);
        if (!value.empty() && value.back() == ']') value.pop_back();
        out.push_back({key, value});
    }
    return out;
}

} // namespace

class PropsCommand : public ICommand {
public:
    std::string name() const override { return "props"; }
    std::string summary() const override { return "List or search Android system properties"; }
    std::string usage() const override { return "systemapp props [filter] [--json]"; }

    int run(const CommandContext& ctx) override {
        std::string raw = shell_capture("getprop 2>/dev/null");
        auto props = parse_getprop_output(raw);

        std::string filter = ctx.args.empty() ? "" : ctx.args[0];
        if (!filter.empty()) {
            props.erase(std::remove_if(props.begin(), props.end(), [&](const PropPair& p) {
                return p.key.find(filter) == std::string::npos;
            }), props.end());
        }

        if (ctx.json_output) {
            auto arr = JsonValue::array();
            for (const auto& p : props) {
                auto obj = JsonValue::object();
                obj.set("key", p.key);
                obj.set("value", p.value);
                arr.push_back(obj);
            }
            std::cout << arr.dump() << "\n";
            return 0;
        }

        using namespace systemapp::utils;
        for (const auto& p : props) {
            std::cout << blue(p.key) << " = " << p.value << "\n";
        }
        std::cout << "\n" << props.size() << " properties" << (filter.empty() ? "" : " (filtered)") << "\n";
        return 0;
    }
};

} // namespace systemapp::commands

SYSTEMAPP_REGISTER_COMMAND(systemapp::commands::PropsCommand);
