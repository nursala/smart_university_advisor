#include "AuthorizationService.h"

AuthenticatedIdentity AuthorizationService::identity(
    const drogon::HttpRequestPtr &request)
{
    AuthenticatedIdentity result{
        request->attributes()->get<int64_t>("user_id"),
        request->attributes()->get<std::string>("user_role"),
        std::nullopt};
    if (result.role == "student")
    {
        result.studentId =
            request->attributes()->get<int64_t>("student_id");
    }
    return result;
}

bool AuthorizationService::isStaff(
    const AuthenticatedIdentity &identity)
{
    return identity.role == "advisor" || identity.role == "admin";
}

bool AuthorizationService::canRecordGrades(
    const AuthenticatedIdentity &identity)
{
    return isStaff(identity);
}

bool AuthorizationService::authorizeStudent(
    const AuthenticatedIdentity &identity,
    int64_t requestedStudentId,
    int64_t &authorizedStudentId,
    std::string &error)
{
    if (isStaff(identity))
    {
        if (requestedStudentId <= 0)
        {
            error = "student_id must be a positive integer";
            return false;
        }
        authorizedStudentId = requestedStudentId;
        return true;
    }
    if (identity.role != "student" || !identity.studentId)
    {
        error = "Student account is not linked to a student record";
        return false;
    }
    authorizedStudentId = *identity.studentId;
    if (requestedStudentId > 0 && requestedStudentId != authorizedStudentId)
    {
        error = "Students may access only their own student record";
        return false;
    }
    return true;
}
