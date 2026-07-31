#pragma once
// SystemApp - utils/Color
// ANSI color helpers, auto-disabled when stdout isn't a TTY or NO_COLOR is set.

#include <string>
#include <cstdlib>
#include <unistd.h>

namespace systemapp::utils {

inline bool color_supported() {
    if (std::getenv("NO_COLOR") != nullptr) return false;
    return ::isatty(fileno(stdout)) != 0;
}

inline std::string colorize(const std::string& text, const char* code) {
    static const bool enabled = color_supported();
    if (!enabled) return text;
    return std::string("\x1b[") + code + "m" + text + "\x1b[0m";
}

inline std::string green(const std::string& s)  { return colorize(s, "32"); }
inline std::string red(const std::string& s)    { return colorize(s, "31"); }
inline std::string yellow(const std::string& s) { return colorize(s, "33"); }
inline std::string blue(const std::string& s)   { return colorize(s, "34"); }
inline std::string bold(const std::string& s)   { return colorize(s, "1"); }

} // namespace systemapp::utils
