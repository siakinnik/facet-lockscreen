// Unit tests of the hashing, the stored secret and the waits after wrong attempts.
#include <cstdio>
#include <cstdlib>
#include <string>

#include "secret.h"

static int fails = 0;
#define CHECK(c)                                                  \
    do {                                                          \
        if (!(c)) {                                               \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
            ++fails;                                              \
        }                                                         \
    } while (0)

int main() {
    using namespace ls;
    // Known answers (FIPS 180-2, RFC 7914 §11).
    CHECK(sha256_hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(pbkdf2_sha256_hex("password", "salt", 1) == "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
    CHECK(pbkdf2_sha256_hex("password", "salt", 4096) == "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a");
    CHECK(pbkdf2_sha256_hex("passwd", "salt", 1, 64) ==
          "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783");
    CHECK(equal_ct("abc", "abc") && !equal_ct("abc", "abd") && !equal_ct("abc", "ab"));

    CHECK(Secret::valid(Kind::Pin, "1234") && !Secret::valid(Kind::Pin, "123") && !Secret::valid(Kind::Pin, "12a4"));
    CHECK(Secret::pattern_text({0, 4, 8, 5}) == "1596");

    char tmpl[] = "/tmp/ls-test-XXXXXX";
    std::string dir = mkdtemp(tmpl);
    {
        Secret s(dir);
        s.load();
        CHECK(s.kind() == Kind::None);
        CHECK(s.check("anything", 1000) == Secret::Result::Ok);  // no lock: nothing to check
        CHECK(!s.set(Kind::Pin, "12"));                          // too short
        CHECK(s.set(Kind::Pin, "2580"));
    }
    {
        Secret s(dir);  // as after a restart
        s.load();
        CHECK(s.kind() == Kind::Pin);
        CHECK(s.check("2580", 1000) == Secret::Result::Ok);
        // A PIN and a password with the same characters are different secrets.
        for (int i = 0; i < 4; ++i) CHECK(s.check("0000", 1000) == Secret::Result::Wrong);
        CHECK(s.wait_seconds(1000) == 0);
        CHECK(s.check("1111", 1000) == Secret::Result::Wrong);  // the 5th miss: a pause
        CHECK(s.wait_seconds(1000) == 30);
        CHECK(s.check("2580", 1010) == Secret::Result::Wait);  // even the right PIN waits
    }
    {
        Secret s(dir);  // a restart does not end the pause
        s.load();
        CHECK(s.wait_seconds(1010) == 20);
        CHECK(s.check("9999", 1031) == Secret::Result::Wrong);  // 6th miss: 60 s
        CHECK(s.wait_seconds(1031) == 60);
        CHECK(s.check("2580", 1100) == Secret::Result::Ok);
        CHECK(s.failures() == 0 && s.wait_seconds(1100) == 0);
        CHECK(s.set(Kind::Password, "2580"));
        CHECK(s.check("2580", 1100) == Secret::Result::Ok);
        CHECK(s.set(Kind::None, {}));
    }
    {
        Secret s(dir);
        s.load();
        CHECK(s.kind() == Kind::None);
    }
    for (const char* f : {"/lock.json", "/attempts.json"}) std::remove((dir + f).c_str());
    std::remove(dir.c_str());
    std::printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
