#pragma once

#include <cstdint>
#include <optional>
#include <string>

// Minimal HS256 JWT issuing/verification for session tokens. No external
// JWT library -- implemented directly on OpenSSL's HMAC (already a hard
// dependency via Drogon's TLS support), the same "thin wrapper instead of
// a heavy SDK" approach GeminiClient uses for the Gemini REST API.
class JwtService
{
  public:
    struct Claims
    {
        int64_t userId;
        std::string role;
    };

    // Reads JWT_SECRET from the environment. Throws std::runtime_error if
    // it is missing/empty -- callers must catch this and turn it into a
    // clean JSON error response, mirroring how GeminiClient's constructor
    // is used in AgentController.
    JwtService();

    // Issues a signed token for `userId`/`role`, valid for `ttlSeconds`
    // (default 24h) from now.
    std::string issue(int64_t userId,
                      const std::string &role,
                      int64_t ttlSeconds = 24 * 60 * 60) const;

    // Verifies the signature and expiry. Returns std::nullopt for any
    // failure (bad signature, malformed token, expired) -- never throws.
    std::optional<Claims> verify(const std::string &token) const;

  private:
    std::string secret_;
};
