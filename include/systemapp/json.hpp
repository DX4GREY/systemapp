#pragma once
// SystemApp - core/JsonWriter
// Minimal dependency-free JSON value builder for --json output.
// Not a general-purpose JSON parser; write-only, sufficient for CLI reporting.

#include <string>
#include <vector>
#include <utility>
#include <sstream>
#include <variant>
#include <memory>

namespace systemapp::core {

class JsonValue {
public:
    using Array = std::vector<JsonValue>;
    using Object = std::vector<std::pair<std::string, JsonValue>>;

    JsonValue() : data_(nullptr) {}
    JsonValue(std::nullptr_t) : data_(nullptr) {}
    JsonValue(bool b) : data_(b) {}
    JsonValue(int i) : data_(static_cast<long long>(i)) {}
    JsonValue(long long i) : data_(i) {}
    JsonValue(double d) : data_(d) {}
    JsonValue(const char* s) : data_(std::string(s)) {}
    JsonValue(std::string s) : data_(std::move(s)) {}
    JsonValue(Array a) : data_(std::move(a)) {}
    JsonValue(Object o) : data_(std::move(o)) {}

    static JsonValue object() { return JsonValue(Object{}); }
    static JsonValue array() { return JsonValue(Array{}); }

    JsonValue& set(const std::string& key, JsonValue value) {
        if (auto* obj = std::get_if<Object>(&data_)) {
            for (auto& kv : *obj) {
                if (kv.first == key) { kv.second = std::move(value); return *this; }
            }
            obj->emplace_back(key, std::move(value));
        }
        return *this;
    }

    void push_back(JsonValue value) {
        if (auto* arr = std::get_if<Array>(&data_)) {
            arr->push_back(std::move(value));
        }
    }

    std::string dump(int indent = 2) const { std::ostringstream os; write(os, indent, 0); return os.str(); }

private:
    std::variant<std::nullptr_t, bool, long long, double, std::string, Array, Object> data_;

    static void escape(std::ostringstream& os, const std::string& s) {
        os << '"';
        for (char c : s) {
            switch (c) {
                case '"': os << "\\\""; break;
                case '\\': os << "\\\\"; break;
                case '\n': os << "\\n"; break;
                case '\t': os << "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8]; std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        os << buf;
                    } else {
                        os << c;
                    }
            }
        }
        os << '"';
    }

    void write(std::ostringstream& os, int indent, int depth) const {
        std::string pad(indent * (depth + 1), ' ');
        std::string pad_close(indent * depth, ' ');
        if (std::holds_alternative<std::nullptr_t>(data_)) { os << "null"; return; }
        if (auto* b = std::get_if<bool>(&data_)) { os << (*b ? "true" : "false"); return; }
        if (auto* i = std::get_if<long long>(&data_)) { os << *i; return; }
        if (auto* d = std::get_if<double>(&data_)) { os << *d; return; }
        if (auto* s = std::get_if<std::string>(&data_)) { escape(os, *s); return; }
        if (auto* arr = std::get_if<Array>(&data_)) {
            if (arr->empty()) { os << "[]"; return; }
            os << "[\n";
            for (size_t i = 0; i < arr->size(); ++i) {
                os << pad; (*arr)[i].write(os, indent, depth + 1);
                if (i + 1 != arr->size()) os << ",";
                os << "\n";
            }
            os << pad_close << "]";
            return;
        }
        if (auto* obj = std::get_if<Object>(&data_)) {
            if (obj->empty()) { os << "{}"; return; }
            os << "{\n";
            for (size_t i = 0; i < obj->size(); ++i) {
                os << pad; escape(os, (*obj)[i].first); os << ": ";
                (*obj)[i].second.write(os, indent, depth + 1);
                if (i + 1 != obj->size()) os << ",";
                os << "\n";
            }
            os << pad_close << "}";
            return;
        }
    }
};

} // namespace systemapp::core
