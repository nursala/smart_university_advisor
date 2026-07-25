#include "EnrollmentsController.h"

#include <cmath>

#include <drogon/drogon.h>

#include "../services/AuthorizationService.h"
#include "../services/EnrollmentService.h"
#include "../services/ServiceResultHttp.h"
#include "../services/ValidationHelpers.h"

namespace
{
drogon::HttpResponsePtr errorResponse(const std::string &message,
                                      drogon::HttpStatusCode statusCode)
{
    Json::Value error;
    error["error"] = message;
    auto response = drogon::HttpResponse::newHttpJsonResponse(error);
    response->setStatusCode(statusCode);
    return response;
}

bool isPositiveInteger(const Json::Value &value, int64_t &out)
{
    std::string error;
    return ValidationHelpers::tryGetInt64(value, out, error) && out > 0;
}
}  // namespace

void EnrollmentsController::create(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) const
{
    const auto &body = request->getJsonObject();
    int64_t studentId = 0;
    int64_t courseId = 0;
    if (!body || !body->isObject() ||
        !body->isMember("course_id") || !body->isMember("semester") ||
        !isPositiveInteger((*body)["course_id"], courseId) ||
        !(*body)["semester"].isString() ||
        (*body)["semester"].asString().empty())
    {
        callback(errorResponse(
            "course_id and a non-empty semester are required",
            drogon::k400BadRequest));
        return;
    }

    if (body->isMember("student_id") &&
        !isPositiveInteger((*body)["student_id"], studentId))
    {
        callback(errorResponse("student_id must be a positive integer",
                               drogon::k400BadRequest));
        return;
    }
    const auto identity = AuthorizationService::identity(request);
    int64_t authorizedStudentId = 0;
    std::string authorizationError;
    if (!AuthorizationService::authorizeStudent(
            identity, studentId, authorizedStudentId, authorizationError))
    {
        callback(errorResponse(authorizationError, drogon::k403Forbidden));
        return;
    }

    EnrollmentService::create(
        drogon::app().getDbClient(),
        authorizedStudentId,
        courseId,
        (*body)["semester"].asString(),
        [callback](ServiceResult result) { callback(toHttpResponse(result)); });
}

void EnrollmentsController::recordGrade(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    int64_t enrollmentId) const
{
    if (!AuthorizationService::canRecordGrades(
            AuthorizationService::identity(request)))
    {
        callback(errorResponse("Students may not record official grades",
                               drogon::k403Forbidden));
        return;
    }
    const auto &body = request->getJsonObject();
    if (!body || !body->isObject() || !body->isMember("grade") ||
        !(*body)["grade"].isNumeric())
    {
        callback(errorResponse("grade must be a number between 0 and 100",
                               drogon::k400BadRequest));
        return;
    }

    const auto grade = (*body)["grade"].asDouble();
    if (!std::isfinite(grade) || grade < 0 || grade > 100)
    {
        callback(errorResponse("grade must be a number between 0 and 100",
                               drogon::k400BadRequest));
        return;
    }

    EnrollmentService::recordGrade(
        drogon::app().getDbClient(),
        enrollmentId,
        grade,
        [callback](ServiceResult result) { callback(toHttpResponse(result)); });
}

void EnrollmentsController::remove(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    int64_t enrollmentId) const
{
    const auto identity = AuthorizationService::identity(request);
    auto database = drogon::app().getDbClient();
    database->execSqlAsync(
        "SELECT student_id, status FROM enrollments WHERE id = $1",
        [database, callback, identity, enrollmentId](
            const drogon::orm::Result &result) {
            if (result.empty())
            {
                callback(toHttpResponse(
                    ServiceResult::notFound("Enrollment not found")));
                return;
            }
            int64_t authorizedStudentId = 0;
            std::string error;
            if (!AuthorizationService::authorizeStudent(
                    identity,
                    result.front()["student_id"].as<int64_t>(),
                    authorizedStudentId,
                    error))
            {
                callback(errorResponse(error, drogon::k403Forbidden));
                return;
            }
            if (!AuthorizationService::isStaff(identity) &&
                result.front()["status"].as<std::string>() != "planned")
            {
                callback(errorResponse(
                    "Students may delete only planned enrollments",
                    drogon::k403Forbidden));
                return;
            }
            EnrollmentService::remove(
                database,
                enrollmentId,
                [callback](ServiceResult serviceResult) {
                    callback(toHttpResponse(serviceResult));
                });
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to authorize enrollment deletion: "
                      << exception.base().what();
            callback(errorResponse("Unable to authorize enrollment",
                                   drogon::k500InternalServerError));
        },
        enrollmentId);
}
