#include <drogon/drogon.h>

#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
std::string envOrDefault(const char *name, const char *defaultValue)
{
    const char *value = std::getenv(name);
    return value != nullptr && value[0] != '\0' ? value : defaultValue;
}

unsigned short databasePort()
{
    const auto portValue = envOrDefault("DB_PORT", "5432");
    const auto port = std::stoul(portValue);
    if (port > std::numeric_limits<unsigned short>::max())
    {
        throw std::out_of_range("DB_PORT must be between 0 and 65535");
    }
    return static_cast<unsigned short>(port);
}

// Matches `number_of_threads` in config.json by default: one DB
// connection per IO thread avoids threads blocking on each other for a
// pooled connection under concurrent load.
std::size_t connectionPoolSize()
{
    const auto poolSizeValue = envOrDefault("DB_POOL_SIZE", "4");
    const auto poolSize = std::stoul(poolSizeValue);
    if (poolSize == 0)
    {
        throw std::out_of_range("DB_POOL_SIZE must be a positive integer");
    }
    return poolSize;
}
}  // namespace

int main()
{
    auto &application = drogon::app();
    application.loadConfigFile("./config.json");

    application.addDbClient(drogon::orm::DbConfig{
        drogon::orm::PostgresConfig{
            envOrDefault("DB_HOST", "db"),
            databasePort(),
            envOrDefault("DB_NAME", "smart_university_advisor"),
            envOrDefault("DB_USER", "advisor"),
            envOrDefault("DB_PASSWORD", "advisor_password"),
            connectionPoolSize(),
            "default",
            false,
            "",
            -1.0,
            false,
            {}}});

    application.run();
    return 0;
}
