#pragma once

#include <drogon/HttpRequest.h>

#include <cstdint>
#include <optional>
#include <string>

struct AuthenticatedIdentity
{
    int64_t userId;
    std::string role;
    std::optional<int64_t> studentId;
};

class AuthorizationService
{
  public:
    static AuthenticatedIdentity identity(
        const drogon::HttpRequestPtr &request);
    static bool isStaff(const AuthenticatedIdentity &identity);
    static bool canRecordGrades(const AuthenticatedIdentity &identity);
    static bool authorizeStudent(
        const AuthenticatedIdentity &identity,
        int64_t requestedStudentId,
        int64_t &authorizedStudentId,
        std::string &error);
};
