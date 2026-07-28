#pragma once

#include <string>

// Password hashing via PBKDF2-HMAC-SHA256 (OpenSSL). Chosen over bcrypt
// specifically to avoid adding a new system package: OpenSSL is already a
// hard requirement for Drogon's TLS support, so this needs nothing beyond
// what the base image already provides. PBKDF2-HMAC-SHA256 is an
// OWASP-recommended password hashing scheme -- the spec's "hash function
// (e.g. bcrypt)" names bcrypt as an example, not a hard requirement.
//
// Output format: "pbkdf2_sha256$<iterations>$<salt-b64>$<hash-b64>" -- a
// single self-describing string, so the iteration count can be raised
// later without invalidating hashes stored with the old value.
//
// NOTE: database/seed.sql now stores real hashes produced by this scheme
// (pbkdf2_sha256$210000$...), not the earlier decorative bcrypt-shaped
// placeholders. The seeded demo accounts (adam@example.com, admin@example.com)
// log in with their documented README passwords with no extra setup, as
// long as the Postgres data volume was initialized from the current
// database/seed.sql (see docs/development-database-reset.md if an older
// volume is still in use).
class PasswordHasher
{
  public:
    // Hashes `password` with a fresh random 16-byte salt.
    // Throws std::runtime_error if OpenSSL's RNG or KDF call fails.
    static std::string hash(const std::string &password);

    // Returns true if `password` matches `encoded` (as produced by hash()).
    // Never throws; a malformed/unrecognized hash is treated as a
    // non-match rather than an error.
    static bool verify(const std::string &password, const std::string &encoded);

  private:
    static constexpr int kIterations = 210000;
    static constexpr int kSaltBytes = 16;
    static constexpr int kHashBytes = 32;
};
