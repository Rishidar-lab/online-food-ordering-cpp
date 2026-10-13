#ifndef OFOS_JSON_HPP
#define OFOS_JSON_HPP

#include "utils.hpp"
#include <string>
#include <sstream>
#include <vector>

namespace ofos {

// Minimal fluent JSON object/array builder for AJAX responses.
// Not a parser — this project only needs to *emit* JSON.
class Json {
public:
    Json& str(const std::string& key, const std::string& value) {
        add_key(key);
        buf_ << '"' << util::json_escape(value) << '"';
        return *this;
    }
    Json& num(const std::string& key, double value) {
        add_key(key);
        buf_ << util::money(value);
        return *this;
    }
    Json& integer(const std::string& key, long long value) {
        add_key(key);
        buf_ << value;
        return *this;
    }
    Json& boolean(const std::string& key, bool value) {
        add_key(key);
        buf_ << (value ? "true" : "false");
        return *this;
    }
    // Insert a raw, already-valid JSON fragment (e.g. a nested array built
    // elsewhere).
    Json& raw(const std::string& key, const std::string& json_fragment) {
        add_key(key);
        buf_ << json_fragment;
        return *this;
    }

    std::string dump() const { return "{" + buf_.str() + "}"; }

    // Helpers for ad-hoc responses.
    static std::string ok(const std::string& message = "") {
        Json j; j.boolean("success", true);
        if (!message.empty()) j.str("message", message);
        return j.dump();
    }
    static std::string error(const std::string& message) {
        Json j; j.boolean("success", false).str("error", message);
        return j.dump();
    }

private:
    void add_key(const std::string& key) {
        if (first_) first_ = false; else buf_ << ',';
        buf_ << '"' << util::json_escape(key) << "\":";
    }
    std::ostringstream buf_;
    bool first_ = true;
};

} // namespace ofos

#endif // OFOS_JSON_HPP
