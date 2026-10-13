#ifndef OFOS_SHA256_HPP
#define OFOS_SHA256_HPP

#include <string>
#include <cstdint>

// Minimal, dependency-free SHA-256 (FIPS 180-4).
// Used only for salted password hashing in this project; for production
// a memory-hard KDF (bcrypt / argon2) is preferred (see docs/DESIGN.md).
namespace ofos {

class Sha256 {
public:
    Sha256();
    void update(const uint8_t* data, size_t len);
    void update(const std::string& s);
    // Finalises and returns the 64-char lowercase hex digest. The object
    // must not be reused afterwards without calling reset().
    std::string hex_digest();
    void reset();

private:
    void transform(const uint8_t* chunk);

    uint32_t state_[8];
    uint64_t bitlen_;
    uint8_t  buffer_[64];
    size_t   buffer_len_;
};

// Convenience one-shot helper.
std::string sha256_hex(const std::string& data);

} // namespace ofos

#endif // OFOS_SHA256_HPP
