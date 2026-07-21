#include "CoursesController.h"

#include <drogon/drogon.h>

void CoursesController::list(
    const drogon::HttpRequestPtr &,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) const
{
    auto database = drogon::app().getDbClient();
    database->execSqlAsync(
        "SELECT id, code, name, department, credits, difficulty_level "
        "FROM courses ORDER BY id",
        [callback](const drogon::orm::Result &result) {
            Json::Value courses(Json::arrayValue);
            for (const auto &row : result)
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
                courses.append(std::move(course));
            }

            callback(drogon::HttpResponse::newHttpJsonResponse(courses));
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to load courses: " << exception.base().what();
            Json::Value error;
            error["error"] = "Unable to load courses";
            auto response = drogon::HttpResponse::newHttpJsonResponse(error);
            response->setStatusCode(drogon::k500InternalServerError);
            callback(response);
        });
}

void CoursesController::details(
    const drogon::HttpRequestPtr &,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    int64_t courseId) const
{
    auto database = drogon::app().getDbClient();
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
                Json::Value error;
                error["error"] = "Course not found";
                auto response = drogon::HttpResponse::newHttpJsonResponse(error);
                response->setStatusCode(drogon::k404NotFound);
                callback(response);
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
                    callback(drogon::HttpResponse::newHttpJsonResponse(course));
                },
                [callback](const drogon::orm::DrogonDbException &exception) {
                    LOG_ERROR << "Failed to load course prerequisites: "
                              << exception.base().what();
                    Json::Value error;
                    error["error"] = "Unable to load course details";
                    auto response =
                        drogon::HttpResponse::newHttpJsonResponse(error);
                    response->setStatusCode(drogon::k500InternalServerError);
                    callback(response);
                },
                courseId);
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to load course details: "
                      << exception.base().what();
            Json::Value error;
            error["error"] = "Unable to load course details";
            auto response = drogon::HttpResponse::newHttpJsonResponse(error);
            response->setStatusCode(drogon::k500InternalServerError);
            callback(response);
        },
        courseId);
}
