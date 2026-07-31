// SystemApp - commands/ls
// systemapp ls <path> [--json]
// First of the internal filesystem toolset (cp/mv/rm/mkdir/touch/cat/stat/
// find/du/df/tree land the same way, one file each under src/fs, wired up
// as commands here). Uses POSIX dirent/stat directly, no shelling out.

#include "systemapp/command.hpp"
#include "systemapp/json.hpp"
#include "systemapp/color.hpp"

#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstring>

namespace systemapp::commands {

using systemapp::core::CommandContext;
using systemapp::core::ICommand;
using systemapp::core::JsonValue;

namespace {

std::string mode_string(mode_t m) {
    std::string s(10, '-');
    if (S_ISDIR(m)) s[0] = 'd';
    else if (S_ISLNK(m)) s[0] = 'l';
    else if (S_ISCHR(m)) s[0] = 'c';
    else if (S_ISBLK(m)) s[0] = 'b';
    else if (S_ISFIFO(m)) s[0] = 'p';
    else if (S_ISSOCK(m)) s[0] = 's';

    const char* bits = "rwxrwxrwx";
    for (int i = 0; i < 9; ++i) {
        s[1 + i] = (m & (1 << (8 - i))) ? bits[i] : '-';
    }
    return s;
}

} // namespace

class LsCommand : public ICommand {
public:
    std::string name() const override { return "ls"; }
    std::string summary() const override { return "List directory contents"; }
    std::string usage() const override { return "systemapp ls <path> [--json]"; }

    int run(const CommandContext& ctx) override {
        std::string path = ctx.args.empty() ? "." : ctx.args[0];

        DIR* dir = opendir(path.c_str());
        if (!dir) {
            std::cerr << systemapp::utils::red("error: ") << "cannot open '" << path
                       << "': " << std::strerror(errno) << "\n";
            return 1;
        }

        struct Entry { std::string name; struct stat st; bool has_stat; };
        std::vector<Entry> entries;

        struct dirent* de;
        while ((de = readdir(dir)) != nullptr) {
            std::string fname = de->d_name;
            if (fname == "." || fname == "..") continue;
            struct stat st{};
            std::string full = path + "/" + fname;
            bool ok = (lstat(full.c_str(), &st) == 0);
            entries.push_back({fname, st, ok});
        }
        closedir(dir);

        std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
            return a.name < b.name;
        });

        if (ctx.json_output) {
            auto arr = JsonValue::array();
            for (const auto& e : entries) {
                auto obj = JsonValue::object();
                obj.set("name", e.name);
                if (e.has_stat) {
                    obj.set("size", static_cast<long long>(e.st.st_size));
                    obj.set("mode", mode_string(e.st.st_mode));
                    obj.set("uid", static_cast<int>(e.st.st_uid));
                    obj.set("gid", static_cast<int>(e.st.st_gid));
                    obj.set("is_dir", S_ISDIR(e.st.st_mode));
                    obj.set("is_symlink", S_ISLNK(e.st.st_mode));
                }
                arr.push_back(obj);
            }
            std::cout << arr.dump() << "\n";
            return 0;
        }

        using namespace systemapp::utils;
        for (const auto& e : entries) {
            if (!e.has_stat) { std::cout << e.name << "\n"; continue; }
            std::string display = e.name;
            if (S_ISDIR(e.st.st_mode)) display = blue(e.name + "/");
            else if (S_ISLNK(e.st.st_mode)) display = e.name + "@";

            std::cout << mode_string(e.st.st_mode) << "  "
                       << e.st.st_uid << ":" << e.st.st_gid << "  "
                       << e.st.st_size << "\t" << display << "\n";
        }
        return 0;
    }
};

} // namespace systemapp::commands

SYSTEMAPP_REGISTER_COMMAND(systemapp::commands::LsCommand);
