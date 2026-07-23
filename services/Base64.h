#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Minimal base64 codec, shared by JwtService (base64url, unpadded, per the
// JWT spec) and PasswordHasher (standard base64, padded) so neither
// reimplements the same bit-shuffling twice.
class Base64
{
  public:
    static std::string encode(const unsigned char *data,
                              size_t length,
                              bool urlSafe,
                              bool padding);

    static std::vector<unsigned char> decode(const std::string &input,
                                             bool urlSafe);
};
