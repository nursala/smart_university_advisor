#include "CoursesController.h"

#include <drogon/drogon.h>

#include "../services/CourseService.h"
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
}  // namespace

void CoursesController::list(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) const
{
    CourseSearchFilters filters;

    const auto department = request->getParameter("department");
    if (!department.empty())
    {
        filters.department = department;
    }

    const auto difficulty = request->getParameter("difficulty");
    if (!difficulty.empty())
    {
        std::string difficultyError;
        if (!ValidationHelpers::validateDifficultyString(
                difficulty, "difficulty", difficultyError))
        {
            callback(errorResponse(difficultyError, drogon::k400BadRequest));
            return;
        }
        filters.difficulty = difficulty;
    }

    const auto creditsParam = request->getParameter("credits");
    if (!creditsParam.empty())
    {
        try
        {
            size_t consumed = 0;
            const auto credits = std::stoi(creditsParam, &consumed);
            if (consumed != creditsParam.size())
            {
                throw std::invalid_argument("trailing characters");
            }
            filters.credits = credits;
        }
        catch (const std::exception &)
        {
            callback(errorResponse("credits must be an integer",
                                   drogon::k400BadRequest));
            return;
        }
    }

    const auto instructor = request->getParameter("instructor");
    if (!instructor.empty())
    {
        filters.instructor = instructor;
    }

    CourseService::searchCourses(
        drogon::app().getDbClient(), filters, [callback](ServiceResult result) {
            callback(toHttpResponse(result));
        });
}

void CoursesController::details(
    const drogon::HttpRequestPtr &,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    int64_t courseId) const
{
    CourseService::getCourseDetails(
        drogon::app().getDbClient(), courseId, [callback](ServiceResult result) {
            callback(toHttpResponse(result));
        });
}
