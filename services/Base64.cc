#include "Base64.h"

#include <cstdint>

namespace
{
constexpr char kStandardTable[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
constexpr char kUrlSafeTable[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
}  // namespace

std::string Base64::encode(const unsigned char *data,
                           size_t length,
                           bool urlSafe,
                           bool padding)
{
    const char *table = urlSafe ? kUrlSafeTable : kStandardTable;
    std::string encoded;
    encoded.reserve(((length + 2) / 3) * 4);

    size_t i = 0;
    while (i + 3 <= length)
    {
        const uint32_t chunk = (static_cast<uint32_t>(data[i]) << 16) |
                                (static_cast<uint32_t>(data[i + 1]) << 8) |
                                static_cast<uint32_t>(data[i + 2]);
        encoded.push_back(table[(chunk >> 18) & 0x3F]);
        encoded.push_back(table[(chunk >> 12) & 0x3F]);
        encoded.push_back(table[(chunk >> 6) & 0x3F]);
        encoded.push_back(table[chunk & 0x3F]);
        i += 3;
    }

    const size_t remaining = length - i;
    if (remaining == 1)
    {
        const uint32_t chunk = static_cast<uint32_t>(data[i]) << 16;
        encoded.push_back(table[(chunk >> 18) & 0x3F]);
        encoded.push_back(table[(chunk >> 12) & 0x3F]);
        if (padding)
        {
            encoded.append("==");
        }
    }
    else if (remaining == 2)
    {
        const uint32_t chunk = (static_cast<uint32_t>(data[i]) << 16) |
                                (static_cast<uint32_t>(data[i + 1]) << 8);
        encoded.push_back(table[(chunk >> 18) & 0x3F]);
        encoded.push_back(table[(chunk >> 12) & 0x3F]);
        encoded.push_back(table[(chunk >> 6) & 0x3F]);
        if (padding)
        {
            encoded.push_back('=');
        }
    }
    return encoded;
}

std::vector<unsigned char> Base64::decode(const std::string &input,
                                          bool urlSafe)
{
    const std::string table = urlSafe ? kUrlSafeTable : kStandardTable;
    std::vector<unsigned char> decoded;
    decoded.reserve((input.size() / 4) * 3);

    int val = 0;
    int bits = -8;
    for (unsigned char c : input)
    {
        if (c == '=')
        {
            break;
        }
        const auto pos = table.find(static_cast<char>(c));
        if (pos == std::string::npos)
        {
            continue;
        }
        val = (val << 6) + static_cast<int>(pos);
        bits += 6;
        if (bits >= 0)
        {
            decoded.push_back(
                static_cast<unsigned char>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return decoded;
}
