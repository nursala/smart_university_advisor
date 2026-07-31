#include "StudentService.h"

#include <cmath>

#include <drogon/drogon.h>

bool StudentService::isValidDifficulty(const std::string &difficulty)
{
    return difficulty == "easy" || difficulty == "medium" ||
           difficulty == "hard";
}

void StudentService::getProfile(
    const drogon::orm::DbClientPtr &database,
    int64_t studentId,
    std::function<void(ServiceResult)> &&callback)
{
    database->execSqlAsync(
        "SELECT s.id, s.student_number, s.department, s.year_level, "
        "s.current_gpa, s.max_weekly_credits, u.name, u.email "
        "FROM students s "
        "JOIN users u ON u.id = s.user_id "
        "WHERE s.id = $1",
        [callback](const drogon::orm::Result &result) {
            if (result.empty())
            {
                callback(ServiceResult::notFound("Student not found"));
                return;
            }

            const auto &row = result.front();
            Json::Value student;
            student["id"] = Json::Int64(row["id"].as<int64_t>());
            student["student_number"] =
                row["student_number"].as<std::string>();
            student["department"] = row["department"].as<std::string>();
            student["year_level"] = row["year_level"].as<int>();
            student["current_gpa"] = row["current_gpa"].isNull()
                                         ? Json::Value(Json::nullValue)
                                         : Json::Value(
                                               std::round(
                                                   row["current_gpa"].as<double>() *
                                                   100.0) /
                                               100.0);
            student["max_weekly_credits"] =
                row["max_weekly_credits"].as<int>();
            student["name"] = row["name"].as<std::string>();
            student["email"] = row["email"].as<std::string>();
            callback(ServiceResult::ok(std::move(student)));
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to load student profile: "
                      << exception.base().what();
            callback(ServiceResult::error("Unable to load student profile"));
        },
        studentId);
}

void StudentService::getAcademicSummary(
    const drogon::orm::DbClientPtr &database,
    int64_t studentId,
    std::function<void(ServiceResult)> &&callback)
{
    database->execSqlAsync(
        "SELECT s.current_gpa, e.id AS enrollment_id, e.course_id, "
        "c.code AS course_code, c.name AS course_name, e.semester, "
        "c.credits, e.status, g.grade, g.passed "
        "FROM students s "
        "LEFT JOIN enrollments e ON e.student_id = s.id "
        "LEFT JOIN grades g ON g.enrollment_id = e.id "
        "LEFT JOIN courses c ON c.id = e.course_id "
        "WHERE s.id = $1 "
        "ORDER BY e.id",
        [callback](const drogon::orm::Result &result) {
            if (result.empty())
            {
                callback(ServiceResult::notFound("Student not found"));
                return;
            }

            Json::Value summary;
            summary["current_gpa"] = result.front()["current_gpa"].isNull()
                                         ? Json::Value(Json::nullValue)
                                         : Json::Value(
                                               std::round(
                                                   result.front()["current_gpa"]
                                                       .as<double>() *
                                                   100.0) /
                                               100.0);
            Json::Value completed(Json::arrayValue);
            Json::Value active(Json::arrayValue);
            Json::Value planned(Json::arrayValue);
            int64_t completedCount = 0;
            int64_t activeCount = 0;
            int64_t plannedCount = 0;
            int64_t failedCount = 0;
            int64_t completedCredits = 0;

            for (const auto &row : result)
            {
                if (row["enrollment_id"].isNull())
                    continue;

                Json::Value course;
                course["enrollment_id"] =
                    Json::Int64(row["enrollment_id"].as<int64_t>());
                course["course_id"] =
                    Json::Int64(row["course_id"].as<int64_t>());
                course["course_code"] =
                    row["course_code"].as<std::string>();
                course["course_name"] =
                    row["course_name"].as<std::string>();
                course["semester"] = row["semester"].as<std::string>();
                course["credits"] = row["credits"].as<int>();
                const auto status = row["status"].as<std::string>();
                course["status"] = status;

                if (status == "planned")
                {
                    ++plannedCount;
                    planned.append(std::move(course));
                }
                else if (status == "active")
                {
                    ++activeCount;
                    active.append(std::move(course));
                }
                else if (status == "completed")
                {
                    course["grade"] = row["grade"].isNull()
                                          ? Json::Value(Json::nullValue)
                                          : Json::Value(
                                                row["grade"].as<double>());
                    course["passed"] = row["passed"].isNull()
                                           ? Json::Value(Json::nullValue)
                                           : Json::Value(
                                                 row["passed"].as<bool>());
                    if (!row["passed"].isNull() &&
                        row["passed"].as<bool>())
                    {
                        ++completedCount;
                        completedCredits += row["credits"].as<int64_t>();
                    }
                    else if (!row["passed"].isNull())
                    {
                        ++failedCount;
                    }
                    completed.append(std::move(course));
                }
            }

            summary["completed_courses_count"] =
                Json::Int64(completedCount);
            summary["active_courses_count"] = Json::Int64(activeCount);
            summary["planned_courses_count"] = Json::Int64(plannedCount);
            summary["failed_courses_count"] = Json::Int64(failedCount);
            summary["completed_credits"] = Json::Int64(completedCredits);
            summary["completed_courses"] = std::move(completed);
            summary["active_courses"] = std::move(active);
            summary["planned_courses"] = std::move(planned);
            callback(ServiceResult::ok(std::move(summary)));
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to load academic summary: "
                      << exception.base().what();
            callback(
                ServiceResult::error("Unable to load academic summary"));
        },
        studentId);
}

void StudentService::verifyExists(
    const drogon::orm::DbClientPtr &database,
    int64_t studentId,
    std::function<void(ServiceResult)> &&callback)
{
    database->execSqlAsync(
        "SELECT id FROM students WHERE id = $1",
        [callback](const drogon::orm::Result &students) {
            if (students.empty())
            {
                callback(ServiceResult::notFound("Student not found"));
                return;
            }
            callback(ServiceResult::ok(Json::Value()));
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to validate student: "
                      << exception.base().what();
            callback(ServiceResult::error("Unable to run risk analysis"));
        },
        studentId);
}
