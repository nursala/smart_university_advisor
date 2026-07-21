#include "StudentsController.h"

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
}  // namespace

void StudentsController::profile(
    const drogon::HttpRequestPtr &,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    int64_t studentId) const
{
    auto database = drogon::app().getDbClient();
    database->execSqlAsync(
        "SELECT s.id, s.student_number, s.department, s.year_level, "
        "s.current_gpa, s.max_weekly_credits, u.name, u.email "
        "FROM students s "
        "JOIN users u ON u.id = s.user_id "
        "WHERE s.id = $1",
        [callback](const drogon::orm::Result &result) {
            if (result.empty())
            {
                callback(errorResponse("Student not found", drogon::k404NotFound));
                return;
            }

            const auto &row = result.front();
            Json::Value student;
            student["id"] = Json::Int64(row["id"].as<int64_t>());
            student["student_number"] =
                row["student_number"].as<std::string>();
            student["department"] = row["department"].as<std::string>();
            student["year_level"] = row["year_level"].as<int>();
            student["current_gpa"] = row["current_gpa"].as<double>();
            student["max_weekly_credits"] =
                row["max_weekly_credits"].as<int>();
            student["name"] = row["name"].as<std::string>();
            student["email"] = row["email"].as<std::string>();
            callback(drogon::HttpResponse::newHttpJsonResponse(student));
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to load student profile: "
                      << exception.base().what();
            callback(errorResponse("Unable to load student profile",
                                   drogon::k500InternalServerError));
        },
        studentId);
}

void StudentsController::academicSummary(
    const drogon::HttpRequestPtr &,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    int64_t studentId) const
{
    auto database = drogon::app().getDbClient();
    database->execSqlAsync(
        "SELECT s.current_gpa, "
        "COUNT(e.id) FILTER (WHERE e.status = 'completed' "
        "AND g.passed = TRUE) AS completed_courses_count, "
        "COUNT(e.id) FILTER (WHERE e.status = 'active') "
        "AS active_courses_count, "
        "COUNT(e.id) FILTER (WHERE e.status = 'completed' "
        "AND g.passed = FALSE) AS failed_courses_count, "
        "COALESCE(SUM(c.credits) FILTER (WHERE e.status = 'completed' "
        "AND g.passed = TRUE), 0) AS completed_credits "
        "FROM students s "
        "LEFT JOIN enrollments e ON e.student_id = s.id "
        "LEFT JOIN grades g ON g.enrollment_id = e.id "
        "LEFT JOIN courses c ON c.id = e.course_id "
        "WHERE s.id = $1 "
        "GROUP BY s.id, s.current_gpa",
        [callback](const drogon::orm::Result &result) {
            if (result.empty())
            {
                callback(errorResponse("Student not found", drogon::k404NotFound));
                return;
            }

            const auto &row = result.front();
            Json::Value summary;
            summary["current_gpa"] = row["current_gpa"].as<double>();
            summary["completed_courses_count"] = Json::Int64(
                row["completed_courses_count"].as<int64_t>());
            summary["active_courses_count"] =
                Json::Int64(row["active_courses_count"].as<int64_t>());
            summary["failed_courses_count"] =
                Json::Int64(row["failed_courses_count"].as<int64_t>());
            summary["completed_credits"] =
                Json::Int64(row["completed_credits"].as<int64_t>());
            callback(drogon::HttpResponse::newHttpJsonResponse(summary));
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to load academic summary: "
                      << exception.base().what();
            callback(errorResponse("Unable to load academic summary",
                                   drogon::k500InternalServerError));
        },
        studentId);
}

void StudentsController::availableCourses(
    const drogon::HttpRequestPtr &,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    int64_t studentId) const
{
    auto database = drogon::app().getDbClient();
    database->execSqlAsync(
        "SELECT id FROM students WHERE id = $1",
        [database, callback, studentId](const drogon::orm::Result &students) {
            if (students.empty())
            {
                callback(errorResponse("Student not found", drogon::k404NotFound));
                return;
            }

            database->execSqlAsync(
                "SELECT c.id, c.code, c.name, c.department, c.credits, "
                "c.difficulty_level "
                "FROM courses c "
                "WHERE NOT EXISTS ("
                "SELECT 1 FROM enrollments e "
                "WHERE e.student_id = $1 AND e.course_id = c.id "
                "AND e.status IN ('active', 'completed')) "
                "AND NOT EXISTS ("
                "SELECT 1 FROM course_prerequisites cp "
                "WHERE cp.course_id = c.id "
                "AND NOT EXISTS ("
                "SELECT 1 FROM enrollments e "
                "JOIN grades g ON g.enrollment_id = e.id "
                "WHERE e.student_id = $2 "
                "AND e.course_id = cp.prerequisite_course_id "
                "AND e.status = 'completed' "
                "AND g.grade >= cp.minimum_grade)) "
                "ORDER BY c.id",
                [callback](const drogon::orm::Result &courses) {
                    Json::Value availableCourses(Json::arrayValue);
                    for (const auto &row : courses)
                    {
                        Json::Value course;
                        course["id"] = Json::Int64(row["id"].as<int64_t>());
                        course["code"] = row["code"].as<std::string>();
                        course["name"] = row["name"].as<std::string>();
                        course["department"] =
                            row["department"].as<std::string>();
                        course["credits"] = row["credits"].as<int>();
                        course["difficulty_level"] =
                            row["difficulty_level"].as<std::string>();
                        availableCourses.append(std::move(course));
                    }
                    callback(
                        drogon::HttpResponse::newHttpJsonResponse(availableCourses));
                },
                [callback](const drogon::orm::DrogonDbException &exception) {
                    LOG_ERROR << "Failed to load available courses: "
                              << exception.base().what();
                    callback(errorResponse("Unable to load available courses",
                                           drogon::k500InternalServerError));
                },
                studentId,
                studentId);
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to validate student: " << exception.base().what();
            callback(errorResponse("Unable to load available courses",
                                   drogon::k500InternalServerError));
        },
        studentId);
}
