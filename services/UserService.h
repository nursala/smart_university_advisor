#pragma once

#include <drogon/orm/DbClient.h>

#include <cstdint>
#include <functional>
#include <string>

#include "ServiceResult.h"

// Business logic + data access for the `users` table. Password hashing is
// delegated to PasswordHasher; JWT issuance stays in AuthController (same
// split GeminiClient/AgentController use -- the service owns the DB, the
// controller owns turning a successful result into a session).
class UserService
{
  public:
    // Creates a new user with role 'student' (role is never client-supplied
    // -- see AuthController). Returns BadRequest if the email is already
    // registered.
    static void registerUser(
        const drogon::orm::DbClientPtr &database,
        const std::string &name,
        const std::string &email,
        const std::string &password,
        std::function<void(ServiceResult)> &&callback);

    // Returns BadRequest (not NotFound) for both "no such email" and
    // "wrong password", so the response never reveals whether an email is
    // registered.
    static void login(
        const drogon::orm::DbClientPtr &database,
        const std::string &email,
        const std::string &password,
        std::function<void(ServiceResult)> &&callback);

    static void getProfile(
        const drogon::orm::DbClientPtr &database,
        int64_t userId,
        std::function<void(ServiceResult)> &&callback);

    // Updates whichever of name/email is provided (hasX flags follow the
    // same convention as CourseSearchFilters). Rejects with BadRequest if
    // the new email is already taken by a different user.
    static void updateProfile(
        const drogon::orm::DbClientPtr &database,
        int64_t userId,
        bool hasName,
        const std::string &name,
        bool hasEmail,
        const std::string &email,
        std::function<void(ServiceResult)> &&callback);
};
