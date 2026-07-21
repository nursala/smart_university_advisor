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
