#include "EnrollmentsController.h"

#include <cmath>

#include <drogon/drogon.h>

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
    if (!body || !body->isObject() || !body->isMember("student_id") ||
        !body->isMember("course_id") || !body->isMember("semester") ||
        !isPositiveInteger((*body)["student_id"], studentId) ||
        !isPositiveInteger((*body)["course_id"], courseId) ||
        !(*body)["semester"].isString() ||
        (*body)["semester"].asString().empty())
    {
        callback(errorResponse(
            "student_id, course_id, and a non-empty semester are required",
            drogon::k400BadRequest));
        return;
    }

    const auto semester = (*body)["semester"].asString();

    EnrollmentService::create(
        drogon::app().getDbClient(),
        studentId,
        courseId,
        semester,
        [callback](ServiceResult result) { callback(toHttpResponse(result)); });
}

void EnrollmentsController::recordGrade(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    int64_t enrollmentId) const
{
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
    const drogon::HttpRequestPtr &,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    int64_t enrollmentId) const
{
    EnrollmentService::remove(
        drogon::app().getDbClient(),
        enrollmentId,
        [callback](ServiceResult result) { callback(toHttpResponse(result)); });
}
