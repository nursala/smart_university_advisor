#include "PasswordHasher.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <sstream>
#include <stdexcept>
#include <vector>

#include "Base64.h"

namespace
{
std::vector<unsigned char> pbkdf2(const std::string &password,
                                  const std::vector<unsigned char> &salt,
                                  int iterations,
                                  int outputBytes)
{
    std::vector<unsigned char> output(outputBytes);
    const auto result = PKCS5_PBKDF2_HMAC(password.data(),
                                          static_cast<int>(password.size()),
                                          salt.data(),
                                          static_cast<int>(salt.size()),
                                          iterations,
                                          EVP_sha256(),
                                          outputBytes,
                                          output.data());
    if (result != 1)
    {
        throw std::runtime_error("PBKDF2 hashing failed");
    }
    return output;
}
}  // namespace

std::string PasswordHasher::hash(const std::string &password)
{
    std::vector<unsigned char> salt(kSaltBytes);
    if (RAND_bytes(salt.data(), kSaltBytes) != 1)
    {
        throw std::runtime_error("Failed to generate random salt");
    }

    const auto derived = pbkdf2(password, salt, kIterations, kHashBytes);

    std::ostringstream encoded;
    encoded << "pbkdf2_sha256$" << kIterations << "$"
            << Base64::encode(salt.data(), salt.size(), false, true) << "$"
            << Base64::encode(derived.data(), derived.size(), false, true);
    return encoded.str();
}

bool PasswordHasher::verify(const std::string &password,
                            const std::string &encoded)
{
    // Format: pbkdf2_sha256$<iterations>$<salt-b64>$<hash-b64>
    const auto firstDollar = encoded.find('$');
    if (firstDollar == std::string::npos)
    {
        return false;
    }
    const auto secondDollar = encoded.find('$', firstDollar + 1);
    if (secondDollar == std::string::npos)
    {
        return false;
    }
    const auto thirdDollar = encoded.find('$', secondDollar + 1);
    if (thirdDollar == std::string::npos)
    {
        return false;
    }

    if (encoded.substr(0, firstDollar) != "pbkdf2_sha256")
    {
        return false;
    }

    int iterations = 0;
    try
    {
        iterations = std::stoi(
            encoded.substr(firstDollar + 1, secondDollar - firstDollar - 1));
    }
    catch (const std::exception &)
    {
        return false;
    }
    if (iterations <= 0)
    {
        return false;
    }

    const auto salt = Base64::decode(
        encoded.substr(secondDollar + 1, thirdDollar - secondDollar - 1),
        false);
    const auto expectedHash =
        Base64::decode(encoded.substr(thirdDollar + 1), false);

    std::vector<unsigned char> actualHash;
    try
    {
        actualHash = pbkdf2(
            password, salt, iterations, static_cast<int>(expectedHash.size()));
    }
    catch (const std::exception &)
    {
        return false;
    }

    if (actualHash.size() != expectedHash.size())
    {
        return false;
    }

    // Constant-time comparison so a failed match can't leak timing
    // information about how many leading bytes were correct.
    unsigned char diff = 0;
    for (size_t i = 0; i < actualHash.size(); ++i)
    {
        diff |= actualHash[i] ^ expectedHash[i];
    }
    return diff == 0;
}
