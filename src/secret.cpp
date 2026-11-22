#include "secret.h"

#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>

#include "facet/json.h"

namespace ls {

namespace {

// ------------------------------------------------------------------ SHA-256

struct Sha256 {
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    uint8_t buf[64];
    size_t used = 0;
    uint64_t bits = 0;

    static uint32_t ror(uint32_t v, int n) { return (v >> n) | (v << (32 - n)); }
    void block(const uint8_t* p) {
        static const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = uint32_t(p[4 * i]) << 24 | uint32_t(p[4 * i + 1]) << 16 | uint32_t(p[4 * i + 2]) << 8 | p[4 * i + 3];
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t t1 = hh + (ror(e, 6) ^ ror(e, 11) ^ ror(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
            uint32_t t2 = (ror(a, 2) ^ ror(a, 13) ^ ror(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            hh = g, g = f, f = e, e = d + t1, d = c, c = b, b = a, a = t1 + t2;
        }
        h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e, h[5] += f, h[6] += g, h[7] += hh;
    }
    void update(const uint8_t* p, size_t n) {
        bits += uint64_t(n) * 8;
        while (n > 0) {
            size_t take = std::min(n, 64 - used);
            std::memcpy(buf + used, p, take);
            used += take, p += take, n -= take;
            if (used == 64) block(buf), used = 0;
        }
    }
    void finish(uint8_t out[32]) {
        uint64_t total = bits;
        uint8_t pad = 0x80, zero = 0;
        update(&pad, 1);
        while (used != 56) update(&zero, 1);
        uint8_t len[8];
        for (int i = 0; i < 8; ++i) len[i] = uint8_t(total >> (56 - 8 * i));
        update(len, 8);
        for (int i = 0; i < 8; ++i)
            for (int j = 0; j < 4; ++j) out[4 * i + j] = uint8_t(h[i] >> (24 - 8 * j));
    }
};

void hmac(const uint8_t* key, size_t key_len, const uint8_t* msg, size_t len, uint8_t out[32]) {
    uint8_t k[64] = {};
    if (key_len > 64) {
        Sha256 s;
        s.update(key, key_len);
        s.finish(k);
    } else {
        std::memcpy(k, key, key_len);
    }
    uint8_t ipad[64], opad[64];
    for (int i = 0; i < 64; ++i) ipad[i] = k[i] ^ 0x36, opad[i] = k[i] ^ 0x5c;
    uint8_t inner[32];
    Sha256 a;
    a.update(ipad, 64);
    a.update(msg, len);
    a.finish(inner);
    Sha256 b;
    b.update(opad, 64);
    b.update(inner, 32);
    b.finish(out);
}

std::string hex(const uint8_t* p, size_t n) {
    static const char* d = "0123456789abcdef";
    std::string out;
    for (size_t i = 0; i < n; ++i) out += d[p[i] >> 4], out += d[p[i] & 15];
    return out;
}

std::string random_hex(size_t bytes) {
    std::vector<uint8_t> b(bytes);
    size_t got = 0;
    while (got < bytes) {
        ssize_t n = getrandom(b.data() + got, bytes - got, 0);
        if (n > 0) got += size_t(n);
    }
    return hex(b.data(), bytes);
}

}  // namespace

const char* kind_name(Kind k) {
    switch (k) {
        case Kind::Pin: return "pin";
        case Kind::Password: return "password";
        case Kind::Pattern: return "pattern";
        default: return "none";
    }
}

Kind kind_from(const std::string& s) {
    if (s == "pin") return Kind::Pin;
    if (s == "password") return Kind::Password;
    if (s == "pattern") return Kind::Pattern;
    return Kind::None;
}

std::string sha256_hex(const std::string& data) {
    Sha256 s;
    s.update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    uint8_t out[32];
    s.finish(out);
    return hex(out, 32);
}

std::string pbkdf2_sha256_hex(const std::string& password, const std::string& salt, int iterations, int len) {
    std::string out;
    const auto* pw = reinterpret_cast<const uint8_t*>(password.data());
    for (uint32_t block = 1; int(out.size()) < len * 2; ++block) {
        std::string s = salt;
        s += char(block >> 24), s += char(block >> 16), s += char(block >> 8), s += char(block);
        uint8_t u[32], t[32];
        hmac(pw, password.size(), reinterpret_cast<const uint8_t*>(s.data()), s.size(), u);
        std::memcpy(t, u, 32);
        for (int i = 1; i < iterations; ++i) {
            hmac(pw, password.size(), u, 32, u);
            for (int j = 0; j < 32; ++j) t[j] ^= u[j];
        }
        out += hex(t, 32);
    }
    return out.substr(0, size_t(len) * 2);
}

bool equal_ct(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    return diff == 0;
}

// ------------------------------------------------------------------ Secret

std::string Secret::input(Kind kind, const std::string& value) const {
    return std::string(kind_name(kind)) + ":" + value;
}

std::string Secret::pattern_text(const std::vector<int>& dots) {
    std::string s;
    for (int d : dots) s += char('1' + d);
    return s;
}

bool Secret::valid(Kind kind, const std::string& value) {
    switch (kind) {
        case Kind::Pin:
            return value.size() >= 4 && value.size() <= 16 &&
                   std::all_of(value.begin(), value.end(), [](char c) { return c >= '0' && c <= '9'; });
        case Kind::Password: return value.size() >= 4 && value.size() <= 256;
        case Kind::Pattern: return value.size() >= 4 && value.size() <= 9;
        default: return true;
    }
}

void Secret::load() {
    facet::Json j;
    kind_ = Kind::None;
    if (facet::load_json_file(dir_ + "/lock.json", j)) {
        kind_ = kind_from(j["kind"].str());
        salt_ = j["salt"].str();
        hash_ = j["hash"].str();
        iterations_ = std::max(1000, j["iterations"].as_int(kIterations));
        if (salt_.empty() || hash_.empty()) kind_ = Kind::None;
    }
    facet::Json st;
    failures_ = 0;
    wait_until_ = 0;
    if (facet::load_json_file(dir_ + "/attempts.json", st)) {
        failures_ = std::max(0, st["failures"].as_int(0));
        wait_until_ = int64_t(st["wait_until"].as_number(0));
    }
}

bool Secret::set(Kind kind, const std::string& value) {
    if (kind != Kind::None && !valid(kind, value)) return false;
    facet::Json j = facet::Json::object();
    if (kind == Kind::None) {
        ::unlink((dir_ + "/lock.json").c_str());
        kind_ = Kind::None;
        salt_.clear(), hash_.clear();
    } else {
        std::string salt = random_hex(16);
        std::string hash = pbkdf2_sha256_hex(input(kind, value), salt, kIterations);
        j["kind"] = kind_name(kind);
        j["salt"] = salt;
        j["hash"] = hash;
        j["iterations"] = kIterations;
        ::umask(077);
        if (!facet::save_json_file(dir_ + "/lock.json", j)) return false;
        ::chmod((dir_ + "/lock.json").c_str(), 0600);
        kind_ = kind, salt_ = salt, hash_ = hash, iterations_ = kIterations;
    }
    failures_ = 0;
    wait_until_ = 0;
    save_state();
    return true;
}

int Secret::wait_seconds(int64_t now) const {
    if (wait_until_ <= now) return 0;
    // A clock set back by more than the longest wait would never end it.
    return int(std::min<int64_t>(wait_until_ - now, 3600));
}

Secret::Result Secret::check(const std::string& value, int64_t now) {
    if (kind_ == Kind::None) return Result::Ok;
    if (wait_seconds(now) > 0) return Result::Wait;
    bool ok = equal_ct(pbkdf2_sha256_hex(input(kind_, value), salt_, iterations_), hash_);
    if (ok) {
        failures_ = 0;
        wait_until_ = 0;
    } else {
        ++failures_;
        if (failures_ >= kFreeAttempts) {
            int over = std::min(failures_ - kFreeAttempts, 7);  // 30 s · 2^7 > an hour
            wait_until_ = now + std::min<int64_t>(int64_t(30) << over, 3600);
        }
    }
    save_state();
    return ok ? Result::Ok : Result::Wrong;
}

void Secret::save_state() const {
    facet::Json st = facet::Json::object();
    st["failures"] = failures_;
    st["wait_until"] = double(wait_until_);
    ::umask(077);
    facet::save_json_file(dir_ + "/attempts.json", st);
}

}  // namespace ls
