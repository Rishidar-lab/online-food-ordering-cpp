#ifndef OFOS_UTILS_HPP
#define OFOS_UTILS_HPP

#include <string>
#include <vector>
#include <map>

namespace ofos {
namespace util {

// --- string helpers ---------------------------------------------------------
std::string trim(const std::string& s);
std::string to_lower(const std::string& s);
std::vector<std::string> split(const std::string& s, char delim);
bool starts_with(const std::string& s, const std::string& prefix);
bool ends_with(const std::string& s, const std::string& suffix);
std::string replace_all(std::string s, const std::string& from, const std::string& to);

// --- encoding / escaping ----------------------------------------------------
std::string url_decode(const std::string& s);           // %xx and '+' -> space
std::string url_encode(const std::string& s);
std::string html_escape(const std::string& s);          // &<>"' for safe output
std::string json_escape(const std::string& s);          // for JSON string values

// Parse "a=1&b=two%20words" into a key->value map (values url-decoded).
std::map<std::string, std::string> parse_urlencoded(const std::string& body);

// --- numbers ----------------------------------------------------------------
long   to_long(const std::string& s, long def = 0);
double to_double(const std::string& s, double def = 0.0);
std::string money(double amount);        // "1234.50"

// --- security / identifiers -------------------------------------------------
std::string random_hex(size_t bytes);    // cryptographically-seeded hex token
// Salted, iterated SHA-256 password hash. Returns the hex digest.
std::string hash_password(const std::string& password, const std::string& salt);
// Constant-time-ish string comparison for secrets.
bool secure_equals(const std::string& a, const std::string& b);

// --- time -------------------------------------------------------------------
std::string now_utc();                   // "YYYY-MM-DD HH:MM:SS" UTC

// --- filesystem -------------------------------------------------------------
bool read_file(const std::string& path, std::string& out);
bool write_file(const std::string& path, const std::string& data);
std::string file_extension(const std::string& name);

} // namespace util
} // namespace ofos

#endif // OFOS_UTILS_HPP
