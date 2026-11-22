// The lock's secret: a PIN, a password or a pattern, kept only as a salted,
// slow hash (PBKDF2-HMAC-SHA256) in the module's data directory, which only
// this module's user can read. Wrong attempts make the next ones wait, also
// across restarts of the module.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ls {

enum class Kind { None, Pin, Password, Pattern };
const char* kind_name(Kind k);  // "none", "pin", "password", "pattern"
Kind kind_from(const std::string& s);

std::string sha256_hex(const std::string& data);
// PBKDF2-HMAC-SHA256, `len` bytes, as hex.
std::string pbkdf2_sha256_hex(const std::string& password, const std::string& salt, int iterations, int len = 32);
// Equal strings, in a time that does not depend on where they differ.
bool equal_ct(const std::string& a, const std::string& b);

class Secret {
public:
    // PBKDF2 rounds for new secrets: about 0.1 s on a slow phone processor.
    static constexpr int kIterations = 20000;
    static constexpr int kFreeAttempts = 5;  // then 30 s, doubling with every further miss, at most an hour

    explicit Secret(std::string dir) : dir_(std::move(dir)) {}
    void load();
    Kind kind() const { return kind_; }

    // A new secret (replaces the old one); None removes it.
    bool set(Kind kind, const std::string& value);

    enum class Result { Ok, Wrong, Wait };
    // Checks an attempt at `now` (seconds of the wall clock). Wait: too many
    // misses, nothing was checked.
    Result check(const std::string& value, int64_t now);
    // Seconds until the next attempt is allowed (0: now).
    int wait_seconds(int64_t now) const;
    int failures() const { return failures_; }
    int attempts_left() const { return failures_ < kFreeAttempts ? kFreeAttempts - failures_ : 0; }

    // A pattern as the text that is hashed: the dots' numbers 1..9, e.g. "1235789".
    static std::string pattern_text(const std::vector<int>& dots);
    static bool valid(Kind kind, const std::string& value);

private:
    std::string input(Kind kind, const std::string& value) const;  // kind-prefixed, so kinds never match each other
    void save_state() const;

    std::string dir_;
    Kind kind_ = Kind::None;
    std::string salt_, hash_;
    int iterations_ = kIterations;
    int failures_ = 0;
    int64_t wait_until_ = 0;
};

}  // namespace ls
