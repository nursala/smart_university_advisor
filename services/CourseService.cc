#include "CourseService.h"

#include <algorithm>
#include <cctype>

#include <drogon/drogon.h>

namespace
{
std::string toLower(const std::string &value)
{
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

bool containsCaseInsensitive(const std::string &haystack,
                             const std::string &needle)
{
    return toLower(haystack).find(toLower(needle)) != std::string::npos;
}
}  // namespace

void CourseService::searchCourses(
    const drogon::orm::DbClientPtr &database,
    const CourseSearchFilters &filters,
    std::function<void(ServiceResult)> &&callback)
{
    database->execSqlAsync(
        "SELECT c.id, c.code, c.name, c.department, c.credits, "
        "c.difficulty_level, i.name AS instructor_name "
        "FROM courses c "
        "LEFT JOIN instructors i ON i.id = c.instructor_id "
        "ORDER BY c.id",
        [callback, filters](const drogon::orm::Result &result) {
            Json::Value courses(Json::arrayValue);
            for (const auto &row : result)
            {
                const auto department = row["department"].as<std::string>();
                const auto difficultyLevel =
                    row["difficulty_level"].as<std::string>();
                const auto credits = row["credits"].as<int>();
                const auto instructorName =
                    row["instructor_name"].isNull()
                        ? std::string()
                        : row["instructor_name"].as<std::string>();

                if (filters.department.has_value() &&
                    toLower(department) != toLower(filters.department.value()))
                {
                    continue;
                }
                if (filters.difficulty.has_value() &&
                    difficultyLevel != filters.difficulty.value())
                {
                    continue;
                }
                if (filters.credits.has_value() &&
                    credits != filters.credits.value())
                {
                    continue;
                }
                if (filters.instructor.has_value() &&
                    !containsCaseInsensitive(instructorName,
                                             filters.instructor.value()))
                {
                    continue;
                }

                Json::Value course;
                course["id"] = Json::Int64(row["id"].as<int64_t>());
                course["code"] = row["code"].as<std::string>();
                course["name"] = row["name"].as<std::string>();
                course["department"] = department;
                course["credits"] = credits;
                course["difficulty_level"] = difficultyLevel;
                courses.append(std::move(course));
            }
            callback(ServiceResult::ok(std::move(courses)));
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to load courses: " << exception.base().what();
            callback(ServiceResult::error("Unable to load courses"));
        });
}

void CourseService::getCourseDetails(
    const drogon::orm::DbClientPtr &database,
    int64_t courseId,
    std::function<void(ServiceResult)> &&callback)
{
    database->execSqlAsync(
        "SELECT c.id, c.code, c.name, c.department, c.credits, "
        "c.difficulty_level, c.estimated_weekly_hours, c.description, "
        "c.instructor_id, i.name AS instructor_name "
        "FROM courses c "
        "LEFT JOIN instructors i ON i.id = c.instructor_id "
        "WHERE c.id = $1",
        [database, callback, courseId](const drogon::orm::Result &result) {
            if (result.empty())
            {
                callback(ServiceResult::notFound("Course not found"));
                return;
            }

            const auto &row = result.front();
            Json::Value course;
            course["id"] = Json::Int64(row["id"].as<int64_t>());
            course["code"] = row["code"].as<std::string>();
            course["name"] = row["name"].as<std::string>();
            course["department"] = row["department"].as<std::string>();
            course["credits"] = row["credits"].as<int>();
            course["difficulty_level"] =
                row["difficulty_level"].as<std::string>();
            course["estimated_weekly_hours"] =
                row["estimated_weekly_hours"].as<int>();
            if (row["description"].isNull())
            {
                course["description"] = Json::nullValue;
            }
            else
            {
                course["description"] = row["description"].as<std::string>();
            }
            if (row["instructor_id"].isNull())
            {
                course["instructor_id"] = Json::nullValue;
                course["instructor_name"] = Json::nullValue;
            }
            else
            {
                course["instructor_id"] =
                    Json::Int64(row["instructor_id"].as<int64_t>());
                course["instructor_name"] =
                    row["instructor_name"].as<std::string>();
            }

            database->execSqlAsync(
                "SELECT prerequisite.code, prerequisite.name, "
                "cp.minimum_grade "
                "FROM course_prerequisites cp "
                "JOIN courses prerequisite "
                "ON prerequisite.id = cp.prerequisite_course_id "
                "WHERE cp.course_id = $1 "
                "ORDER BY prerequisite.id",
                [callback, course = std::move(course)](
                    const drogon::orm::Result &prerequisites) mutable {
                    Json::Value prerequisiteCourses(Json::arrayValue);
                    for (const auto &prerequisite : prerequisites)
                    {
                        Json::Value prerequisiteCourse;
                        prerequisiteCourse["code"] =
                            prerequisite["code"].as<std::string>();
                        prerequisiteCourse["name"] =
                            prerequisite["name"].as<std::string>();
                        prerequisiteCourse["minimum_grade"] =
                            prerequisite["minimum_grade"].as<double>();
                        prerequisiteCourses.append(
                            std::move(prerequisiteCourse));
                    }
                    course["prerequisites"] = std::move(prerequisiteCourses);
                    callback(ServiceResult::ok(std::move(course)));
                },
                [callback](const drogon::orm::DrogonDbException &exception) {
                    LOG_ERROR << "Failed to load course prerequisites: "
                              << exception.base().what();
                    callback(
                        ServiceResult::error("Unable to load course details"));
                },
                courseId);
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to load course details: "
                      << exception.base().what();
            callback(ServiceResult::error("Unable to load course details"));
        },
        courseId);
}
