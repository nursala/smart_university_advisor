#include "EnrollmentsController.h"

#include <cmath>

#include <drogon/drogon.h>

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

bool isPositiveInteger(const Json::Value &value)
{
    return value.isIntegral() && value.asInt64() > 0;
}
}  // namespace

void EnrollmentsController::create(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) const
{
    const auto &body = request->getJsonObject();
    if (!body || !body->isObject() || !body->isMember("student_id") ||
        !body->isMember("course_id") || !body->isMember("semester") ||
        !isPositiveInteger((*body)["student_id"]) ||
        !isPositiveInteger((*body)["course_id"]) ||
        !(*body)["semester"].isString() ||
        (*body)["semester"].asString().empty())
    {
        callback(errorResponse(
            "student_id, course_id, and a non-empty semester are required",
            drogon::k400BadRequest));
        return;
    }

    const auto studentId = (*body)["student_id"].asInt64();
    const auto courseId = (*body)["course_id"].asInt64();
    const auto semester = (*body)["semester"].asString();
    auto database = drogon::app().getDbClient();

    database->execSqlAsync(
        "SELECT id FROM students WHERE id = $1",
        [database, callback, studentId, courseId, semester](
            const drogon::orm::Result &students) {
            if (students.empty())
            {
                callback(errorResponse("Student not found", drogon::k404NotFound));
                return;
            }

            database->execSqlAsync(
                "SELECT id FROM courses WHERE id = $1",
                [database, callback, studentId, courseId, semester](
                    const drogon::orm::Result &courses) {
                    if (courses.empty())
                    {
                        callback(errorResponse("Course not found",
                                               drogon::k404NotFound));
                        return;
                    }

                    database->execSqlAsync(
                        "INSERT INTO enrollments "
                        "(student_id, course_id, semester, status) "
                        "VALUES ($1, $2, $3, 'planned') "
                        "ON CONFLICT (student_id, course_id, semester) "
                        "DO NOTHING "
                        "RETURNING id, student_id, course_id, semester, status, "
                        "enrolled_at::text AS enrolled_at",
                        [callback](const drogon::orm::Result &inserted) {
                            if (inserted.empty())
                            {
                                callback(errorResponse(
                                    "Enrollment already exists for this student, "
                                    "course, and semester",
                                    drogon::k409Conflict));
                                return;
                            }

                            const auto &row = inserted.front();
                            Json::Value enrollment;
                            enrollment["id"] =
                                Json::Int64(row["id"].as<int64_t>());
                            enrollment["student_id"] =
                                Json::Int64(row["student_id"].as<int64_t>());
                            enrollment["course_id"] =
                                Json::Int64(row["course_id"].as<int64_t>());
                            enrollment["semester"] =
                                row["semester"].as<std::string>();
                            enrollment["status"] = row["status"].as<std::string>();
                            enrollment["enrolled_at"] =
                                row["enrolled_at"].as<std::string>();
                            auto response =
                                drogon::HttpResponse::newHttpJsonResponse(enrollment);
                            response->setStatusCode(drogon::k201Created);
                            callback(response);
                        },
                        [callback](const drogon::orm::DrogonDbException &exception) {
                            LOG_ERROR << "Failed to create enrollment: "
                                      << exception.base().what();
                            callback(errorResponse("Unable to create enrollment",
                                                   drogon::k500InternalServerError));
                        },
                        studentId,
                        courseId,
                        semester);
                },
                [callback](const drogon::orm::DrogonDbException &exception) {
                    LOG_ERROR << "Failed to validate course: "
                              << exception.base().what();
                    callback(errorResponse("Unable to create enrollment",
                                           drogon::k500InternalServerError));
                },
                courseId);
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to validate student: " << exception.base().what();
            callback(errorResponse("Unable to create enrollment",
                                   drogon::k500InternalServerError));
        },
        studentId);
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

    const auto passed = grade >= 60;
    auto database = drogon::app().getDbClient();
    database->execSqlAsync(
        "WITH updated_enrollment AS ("
        "UPDATE enrollments SET status = 'completed' "
        "WHERE id = $1 RETURNING id, status), "
        "upserted_grade AS ("
        "INSERT INTO grades (enrollment_id, grade, passed) "
        "SELECT id, $2, $3 FROM updated_enrollment "
        "ON CONFLICT (enrollment_id) DO UPDATE "
        "SET grade = EXCLUDED.grade, passed = EXCLUDED.passed, "
        "graded_at = NOW() "
        "RETURNING enrollment_id, grade, passed) "
        "SELECT ug.enrollment_id, ug.grade, ug.passed, ue.status "
        "FROM upserted_grade ug CROSS JOIN updated_enrollment ue",
        [callback](const drogon::orm::Result &updated) {
            if (updated.empty())
            {
                callback(errorResponse("Enrollment not found", drogon::k404NotFound));
                return;
            }

            const auto &row = updated.front();
            Json::Value result;
            result["enrollment_id"] =
                Json::Int64(row["enrollment_id"].as<int64_t>());
            result["grade"] = row["grade"].as<double>();
            result["passed"] = row["passed"].as<bool>();
            result["status"] = row["status"].as<std::string>();
            callback(drogon::HttpResponse::newHttpJsonResponse(result));
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to record grade: " << exception.base().what();
            callback(errorResponse("Unable to record grade",
                                   drogon::k500InternalServerError));
        },
        enrollmentId,
        grade,
        passed);
}

void EnrollmentsController::remove(
    const drogon::HttpRequestPtr &,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    int64_t enrollmentId) const
{
    auto database = drogon::app().getDbClient();
    database->execSqlAsync(
        "DELETE FROM enrollments WHERE id = $1 RETURNING id",
        [callback](const drogon::orm::Result &deleted) {
            if (deleted.empty())
            {
                callback(errorResponse("Enrollment not found", drogon::k404NotFound));
                return;
            }

            Json::Value result;
            result["id"] = Json::Int64(deleted.front()["id"].as<int64_t>());
            result["deleted"] = true;
            callback(drogon::HttpResponse::newHttpJsonResponse(result));
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to delete enrollment: " << exception.base().what();
            callback(errorResponse("Unable to delete enrollment",
                                   drogon::k500InternalServerError));
        },
        enrollmentId);
}
