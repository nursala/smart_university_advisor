#include "StudentsController.h"

#include <drogon/drogon.h>

#include "../services/ServiceResultHttp.h"
#include "../services/StudentService.h"
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

void StudentsController::profile(
    const drogon::HttpRequestPtr &,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    int64_t studentId) const
{
    StudentService::getProfile(
        drogon::app().getDbClient(), studentId, [callback](ServiceResult result) {
            callback(toHttpResponse(result));
        });
}

void StudentsController::academicSummary(
    const drogon::HttpRequestPtr &,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    int64_t studentId) const
{
    StudentService::getAcademicSummary(
        drogon::app().getDbClient(), studentId, [callback](ServiceResult result) {
            callback(toHttpResponse(result));
        });
}

void StudentsController::availableCourses(
    const drogon::HttpRequestPtr &,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    int64_t studentId) const
{
    StudentService::getAvailableCourses(
        drogon::app().getDbClient(), studentId, [callback](ServiceResult result) {
            callback(toHttpResponse(result));
        });
}

void StudentsController::courseRecommendations(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    int64_t studentId) const
{
    const auto &body = request->getJsonObject();

    bool hasPreferredDifficulty = false;
    std::string preferredDifficulty;
    std::string difficultyError;
    if (body &&
        !ValidationHelpers::tryGetOptionalDifficultyField(
            *body,
            "preferred_difficulty",
            hasPreferredDifficulty,
            preferredDifficulty,
            difficultyError))
    {
        callback(errorResponse(difficultyError, drogon::k400BadRequest));
        return;
    }

    int64_t maxRecommendations = 3;
    if (body && body->isObject() && body->isMember("max_recommendations") &&
        !(*body)["max_recommendations"].isNull())
    {
        std::string parseError;
        if (!ValidationHelpers::tryGetInt64(
                (*body)["max_recommendations"], maxRecommendations, parseError))
        {
            callback(errorResponse("max_recommendations must be an integer",
                                   drogon::k400BadRequest));
            return;
        }
    }

    StudentService::getCourseRecommendations(
        drogon::app().getDbClient(),
        studentId,
        hasPreferredDifficulty,
        preferredDifficulty,
        maxRecommendations,
        [callback](ServiceResult result) { callback(toHttpResponse(result)); });
}

void StudentsController::semesterPlan(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    int64_t studentId) const
{
    const auto &body = request->getJsonObject();

    bool hasPreferredDifficulty = false;
    std::string preferredDifficulty;
    std::string difficultyError;
    if (body &&
        !ValidationHelpers::tryGetOptionalDifficultyField(
            *body,
            "preferred_difficulty",
            hasPreferredDifficulty,
            preferredDifficulty,
            difficultyError))
    {
        callback(errorResponse(difficultyError, drogon::k400BadRequest));
        return;
    }

    bool hasMaxCredits = false;
    int64_t maxCredits = 0;
    if (body && body->isObject() && body->isMember("max_credits") &&
        !(*body)["max_credits"].isNull())
    {
        std::string parseError;
        if (!ValidationHelpers::tryGetInt64(
                (*body)["max_credits"], maxCredits, parseError))
        {
            callback(errorResponse("max_credits must be an integer",
                                   drogon::k400BadRequest));
            return;
        }
        hasMaxCredits = true;
    }

    StudentService::buildSemesterPlan(
        drogon::app().getDbClient(),
        studentId,
        hasPreferredDifficulty,
        preferredDifficulty,
        hasMaxCredits,
        maxCredits,
        [callback](ServiceResult result) { callback(toHttpResponse(result)); });
}

void StudentsController::riskAnalysis(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    int64_t studentId) const
{
    auto database = drogon::app().getDbClient();
    database->execSqlAsync(
        "SELECT id FROM students WHERE id = $1",
        [database, callback, studentId, request](
            const drogon::orm::Result &students) {
            if (students.empty())
            {
                callback(errorResponse("Student not found", drogon::k404NotFound));
                return;
            }

            const auto &body = request->getJsonObject();
            if (!body || !body->isObject() || !body->isMember("course_ids"))
            {
                callback(errorResponse("course_ids is required",
                                       drogon::k400BadRequest));
                return;
            }

            const auto &courseIdsJson = (*body)["course_ids"];
            if (!courseIdsJson.isArray() || courseIdsJson.empty())
            {
                callback(errorResponse("course_ids must be a non-empty list",
                                       drogon::k400BadRequest));
                return;
            }

            std::vector<int64_t> courseIds;
            courseIds.reserve(courseIdsJson.size());
            for (const auto &element : courseIdsJson)
            {
                int64_t courseIdValue = 0;
                std::string parseError;
                if (!ValidationHelpers::tryGetInt64(
                        element, courseIdValue, parseError))
                {
                    callback(errorResponse(
                        "course_ids must contain only integers",
                        drogon::k400BadRequest));
                    return;
                }
                courseIds.push_back(courseIdValue);
            }

            StudentService::analyzeRisk(
                database,
                studentId,
                courseIds,
                [callback](ServiceResult result) {
                    callback(toHttpResponse(result));
                });
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to validate student: " << exception.base().what();
            callback(errorResponse("Unable to run risk analysis",
                                   drogon::k500InternalServerError));
        },
        studentId);
}
