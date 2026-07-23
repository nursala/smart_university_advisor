#include "UserService.h"

#include <drogon/drogon.h>

#include "PasswordHasher.h"

namespace
{
Json::Value toUserJson(const drogon::orm::Row &row)
{
    Json::Value user;
    user["id"] = Json::Int64(row["id"].as<int64_t>());
    user["name"] = row["name"].as<std::string>();
    user["email"] = row["email"].as<std::string>();
    user["role"] = row["role"].as<std::string>();
    return user;
}
}  // namespace

void UserService::registerUser(
    const drogon::orm::DbClientPtr &database,
    const std::string &name,
    const std::string &email,
    const std::string &password,
    std::function<void(ServiceResult)> &&callback)
{
    std::string passwordHash;
    try
    {
        passwordHash = PasswordHasher::hash(password);
    }
    catch (const std::exception &exception)
    {
        LOG_ERROR << "Failed to hash password: " << exception.what();
        callback(ServiceResult::error("Unable to register user"));
        return;
    }

    database->execSqlAsync(
        "INSERT INTO users (name, email, password_hash, role) "
        "VALUES ($1, $2, $3, 'student') "
        "ON CONFLICT (email) DO NOTHING "
        "RETURNING id, name, email, role",
        [callback](const drogon::orm::Result &inserted) {
            if (inserted.empty())
            {
                callback(ServiceResult::badRequest(
                    "An account with this email already exists"));
                return;
            }
            callback(ServiceResult::ok(toUserJson(inserted.front())));
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to register user: "
                      << exception.base().what();
            callback(ServiceResult::error("Unable to register user"));
        },
        name,
        email,
        passwordHash);
}

void UserService::login(
    const drogon::orm::DbClientPtr &database,
    const std::string &email,
    const std::string &password,
    std::function<void(ServiceResult)> &&callback)
{
    database->execSqlAsync(
        "SELECT id, name, email, role, password_hash FROM users "
        "WHERE email = $1",
        [callback, password](const drogon::orm::Result &result) {
            if (result.empty())
            {
                callback(ServiceResult::badRequest("Invalid email or password"));
                return;
            }

            const auto &row = result.front();
            const auto storedHash = row["password_hash"].as<std::string>();
            if (!PasswordHasher::verify(password, storedHash))
            {
                callback(ServiceResult::badRequest("Invalid email or password"));
                return;
            }

            callback(ServiceResult::ok(toUserJson(row)));
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to look up user for login: "
                      << exception.base().what();
            callback(ServiceResult::error("Unable to log in"));
        },
        email);
}

void UserService::getProfile(
    const drogon::orm::DbClientPtr &database,
    int64_t userId,
    std::function<void(ServiceResult)> &&callback)
{
    database->execSqlAsync(
        "SELECT id, name, email, role FROM users WHERE id = $1",
        [callback](const drogon::orm::Result &result) {
            if (result.empty())
            {
                callback(ServiceResult::notFound("User not found"));
                return;
            }
            callback(ServiceResult::ok(toUserJson(result.front())));
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to load user profile: "
                      << exception.base().what();
            callback(ServiceResult::error("Unable to load user profile"));
        },
        userId);
}

void UserService::updateProfile(
    const drogon::orm::DbClientPtr &database,
    int64_t userId,
    bool hasName,
    const std::string &name,
    bool hasEmail,
    const std::string &email,
    std::function<void(ServiceResult)> &&callback)
{
    if (!hasName && !hasEmail)
    {
        callback(ServiceResult::badRequest(
            "At least one of name or email must be provided"));
        return;
    }

    database->execSqlAsync(
        "SELECT name, email FROM users WHERE id = $1",
        [database, callback, userId, hasName, name, hasEmail, email](
            const drogon::orm::Result &existing) {
            if (existing.empty())
            {
                callback(ServiceResult::notFound("User not found"));
                return;
            }

            const auto effectiveName =
                hasName ? name : existing.front()["name"].as<std::string>();
            const auto effectiveEmail =
                hasEmail ? email
                         : existing.front()["email"].as<std::string>();

            auto performUpdate = [database, callback, userId, effectiveName,
                                  effectiveEmail]() {
                database->execSqlAsync(
                    "UPDATE users SET name = $2, email = $3 "
                    "WHERE id = $1 "
                    "RETURNING id, name, email, role",
                    [callback](const drogon::orm::Result &updated) {
                        callback(
                            ServiceResult::ok(toUserJson(updated.front())));
                    },
                    [callback](
                        const drogon::orm::DrogonDbException &exception) {
                        LOG_ERROR << "Failed to update user profile: "
                                  << exception.base().what();
                        callback(ServiceResult::error(
                            "Unable to update user profile"));
                    },
                    userId,
                    effectiveName,
                    effectiveEmail);
            };

            if (!hasEmail)
            {
                performUpdate();
                return;
            }

            // Pre-check email uniqueness explicitly (rather than parsing
            // the UPDATE's unique-violation exception text) -- matches the
            // existence-check-before-write pattern EnrollmentsController
            // already uses for student_id/course_id.
            database->execSqlAsync(
                "SELECT id FROM users WHERE email = $1 AND id <> $2",
                [callback, performUpdate](
                    const drogon::orm::Result &emailOwner) {
                    if (!emailOwner.empty())
                    {
                        callback(ServiceResult::badRequest(
                            "An account with this email already exists"));
                        return;
                    }
                    performUpdate();
                },
                [callback](
                    const drogon::orm::DrogonDbException &exception) {
                    LOG_ERROR << "Failed to check email uniqueness: "
                              << exception.base().what();
                    callback(ServiceResult::error(
                        "Unable to update user profile"));
                },
                email,
                userId);
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to load user for update: "
                      << exception.base().what();
            callback(ServiceResult::error("Unable to update user profile"));
        },
        userId);
}
