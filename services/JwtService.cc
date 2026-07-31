#include "JwtService.h"

#include <openssl/hmac.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>

#include <chrono>
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <stdexcept>

#include <json/json.h>

#include "Base64.h"

namespace
{
std::string envOrEmpty(const char *name)
{
    const char *value = std::getenv(name);
    return value != nullptr ? value : "";
}

std::string hmacSha256(const std::string &key, const std::string &data)
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLength = 0;
    HMAC(EVP_sha256(),
         key.data(),
         static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char *>(data.data()),
         data.size(),
         digest,
         &digestLength);
    return std::string(reinterpret_cast<char *>(digest), digestLength);
}

std::string base64UrlEncodeStr(const std::string &input)
{
    return Base64::encode(
        reinterpret_cast<const unsigned char *>(input.data()),
        input.size(),
        /*urlSafe=*/true,
        /*padding=*/false);
}

constexpr char kHeaderJson[] = R"({"alg":"HS256","typ":"JWT"})";

std::string generateBootId()
{
    unsigned char bytes[32];
    if (RAND_bytes(bytes, sizeof(bytes)) != 1)
        throw std::runtime_error("Unable to generate server boot identifier");
    std::ostringstream output;
    for (const auto byte : bytes)
        output << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<int>(byte);
    return output.str();
}

const std::string &processBootId()
{
    static const std::string value = generateBootId();
    return value;
}

bool constantTimeEqual(const std::string &left, const std::string &right)
{
    return left.size() == right.size() &&
           CRYPTO_memcmp(left.data(), right.data(), left.size()) == 0;
}
}  // namespace

JwtService::JwtService()
    : secret_(envOrEmpty("JWT_SECRET")), bootId_(processBootId())
{
    if (secret_.empty())
    {
        throw std::runtime_error(
            "JWT_SECRET environment variable is missing or empty");
    }
}

JwtService::JwtService(std::string bootIdOverride)
    : secret_(envOrEmpty("JWT_SECRET")), bootId_(std::move(bootIdOverride))
{
    if (secret_.empty())
        throw std::runtime_error(
            "JWT_SECRET environment variable is missing or empty");
    if (bootId_.empty())
        throw std::runtime_error("JWT boot identifier is missing or empty");
}

std::string JwtService::issue(int64_t userId,
                              const std::string &role,
                              int64_t ttlSeconds) const
{
    return issueInternal(userId, role, ttlSeconds, true);
}

std::string JwtService::issueWithoutBootClaimForTesting(
    int64_t userId,
    const std::string &role,
    int64_t ttlSeconds) const
{
    return issueInternal(userId, role, ttlSeconds, false);
}

std::string JwtService::issueInternal(int64_t userId,
                                      const std::string &role,
                                      int64_t ttlSeconds,
                                      bool includeBootId) const
{
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();

    Json::Value payload;
    payload["sub"] = Json::Int64(userId);
    payload["role"] = role;
    if (includeBootId)
        payload["server_boot_id"] = bootId_;
    payload["iat"] = Json::Int64(static_cast<int64_t>(now));
    payload["exp"] = Json::Int64(static_cast<int64_t>(now) + ttlSeconds);

    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";

    const auto headerPart = base64UrlEncodeStr(kHeaderJson);
    const auto payloadPart =
        base64UrlEncodeStr(Json::writeString(writer, payload));

    const auto signingInput = headerPart + "." + payloadPart;
    const auto signature = hmacSha256(secret_, signingInput);
    const auto signaturePart = base64UrlEncodeStr(signature);

    return signingInput + "." + signaturePart;
}

std::optional<JwtService::Claims> JwtService::verify(
    const std::string &token) const
{
    const auto firstDot = token.find('.');
    if (firstDot == std::string::npos)
    {
        return std::nullopt;
    }
    const auto secondDot = token.find('.', firstDot + 1);
    if (secondDot == std::string::npos)
    {
        return std::nullopt;
    }

    const auto headerPart = token.substr(0, firstDot);
    const auto payloadPart =
        token.substr(firstDot + 1, secondDot - firstDot - 1);
    const auto signaturePart = token.substr(secondDot + 1);

    const auto signingInput = headerPart + "." + payloadPart;
    const auto expectedSignature = hmacSha256(secret_, signingInput);
    const auto expectedSignaturePart = base64UrlEncodeStr(expectedSignature);

    if (!constantTimeEqual(signaturePart, expectedSignaturePart))
    {
        return std::nullopt;
    }

    const auto payloadBytes = Base64::decode(payloadPart, /*urlSafe=*/true);
    const std::string payloadJson(payloadBytes.begin(), payloadBytes.end());

    Json::Value payload;
    Json::CharReaderBuilder reader;
    std::string errors;
    std::istringstream stream(payloadJson);
    if (!Json::parseFromStream(reader, stream, &payload, &errors))
    {
        return std::nullopt;
    }

    if (!payload.isMember("sub") || !payload["sub"].isIntegral() ||
        !payload.isMember("role") || !payload["role"].isString() ||
        !payload.isMember("exp") || !payload["exp"].isIntegral() ||
        !payload.isMember("server_boot_id") ||
        !payload["server_boot_id"].isString())
    {
        return std::nullopt;
    }

    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();
    if (payload["exp"].asInt64() < static_cast<int64_t>(now))
    {
        return std::nullopt;
    }
    if (!constantTimeEqual(payload["server_boot_id"].asString(), bootId_))
        return std::nullopt;

    return Claims{payload["sub"].asInt64(), payload["role"].asString()};
}
