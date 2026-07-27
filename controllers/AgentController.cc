#include "AgentController.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>

#include <drogon/drogon.h>

#include "../services/AgentLoop.h"
#include "../services/AuthorizationService.h"
#include "../services/GeminiClient.h"
#include "../services/ToolRegistry.h"
#include "../services/ValidationHelpers.h"

namespace
{
bool asksToModifyPlan(std::string message)
{
    std::transform(message.begin(), message.end(), message.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return message.find("enroll me") != std::string::npos ||
           message.find("enroll in") != std::string::npos ||
           message.find("add me") != std::string::npos ||
           message.find("add to my plan") != std::string::npos ||
           message.find("add it to my plan") != std::string::npos ||
           message.find("remove from my plan") != std::string::npos ||
           message.find("remove it from my plan") != std::string::npos;
}

drogon::HttpResponsePtr errorResponse(const std::string &message,
                                      drogon::HttpStatusCode statusCode)
{
    Json::Value error;
    error["error"] = message;
    auto response = drogon::HttpResponse::newHttpJsonResponse(error);
    response->setStatusCode(statusCode);
    return response;
}

drogon::HttpResponsePtr agentErrorResponse(const std::string &message,
                                           const Json::Value &toolsUsed,
                                           drogon::HttpStatusCode statusCode)
{
    Json::Value error;
    error["error"] = message;
    error["tools_used"] = toolsUsed;
    auto response = drogon::HttpResponse::newHttpJsonResponse(error);
    response->setStatusCode(statusCode);
    return response;
}
}  // namespace

void AgentController::query(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) const
{
    const auto &body = request->getJsonObject();
    int64_t requestedStudentId = 0;
    std::string parseError;
    if (!body || !body->isObject() || !body->isMember("message") ||
        !(*body)["message"].isString() || (*body)["message"].asString().empty())
    {
        callback(errorResponse(
            "a non-empty message is required",
            drogon::k400BadRequest));
        return;
    }

    if (body->isMember("student_id") &&
        !ValidationHelpers::tryGetInt64(
            (*body)["student_id"], requestedStudentId, parseError))
    {
        callback(errorResponse("student_id must be an integer",
                               drogon::k400BadRequest));
        return;
    }
    int64_t studentId = 0;
    std::string authorizationError;
    if (!AuthorizationService::authorizeStudent(
            AuthorizationService::identity(request),
            requestedStudentId,
            studentId,
            authorizationError))
    {
        callback(errorResponse(authorizationError, drogon::k403Forbidden));
        return;
    }

    const auto message = (*body)["message"].asString();
    const auto identity = AuthorizationService::identity(request);
    if (asksToModifyPlan(message))
    {
        Json::Value responseBody;
        responseBody["student_id"] = Json::Int64(studentId);
        responseBody["message"] = message;
        responseBody["answer"] =
            "I cannot modify academic records. Please add or remove the "
            "eligible course yourself from the My Plan page.";
        responseBody["tools_used"] = Json::Value(Json::arrayValue);
        responseBody["status"] = "ok";
        callback(drogon::HttpResponse::newHttpJsonResponse(responseBody));
        return;
    }

    std::shared_ptr<GeminiClient> geminiClient;
    try
    {
        geminiClient = std::make_shared<GeminiClient>();
    }
    catch (const std::exception &exception)
    {
        LOG_ERROR << "Gemini client is not configured: " << exception.what();
        callback(errorResponse(
            std::string("Gemini is not configured: ") + exception.what(),
            drogon::k502BadGateway));
        return;
    }

    Json::Value textPart;
    textPart["text"] =
        "You are an academic advisor assistant. You are helping student_id " +
        std::to_string(studentId) +
        ". Always pass student_id=" + std::to_string(studentId) +
        " when calling tools that require a student_id. Use the available "
        "tools as needed to answer accurately, then give a clear, concise "
        "final answer in plain text. You are strictly read-only: never claim "
        "to add, remove, or change an enrollment. If the student asks you to "
        "enroll, add, remove, or otherwise modify their plan, explain that "
        "you cannot modify academic records and direct them to the My Plan "
        "page, where they must perform the final action themselves.\n\n"
        "Keep academic states distinct: planned means selected in My Plan for "
        "the record's future or specified semester; active means currently "
        "being taken; completed means finished with an official result. Never "
        "describe a planned course as active or completed. Questions about "
        "courses must consider completed_courses, active_courses, and "
        "planned_courses from get_academic_summary. If 'this semester' is "
        "ambiguous, report the semester labels present in the records instead "
        "of inventing the current academic semester.\n\n"
        "Student's question: " + message;
    Json::Value parts(Json::arrayValue);
    parts.append(std::move(textPart));
    Json::Value userTurn;
    userTurn["role"] = "user";
    userTurn["parts"] = std::move(parts);
    Json::Value contents(Json::arrayValue);
    contents.append(std::move(userTurn));

    auto responseCallback = std::make_shared<
        std::function<void(const drogon::HttpResponsePtr &)>>(
        std::move(callback));
    const auto database = drogon::app().getDbClient();

    AgentLoop::start(
        std::move(contents),
        ToolRegistry::toolDeclarations(),
        [geminiClient](
            const Json::Value &conversation,
            const Json::Value &toolDeclarations,
            AgentLoop::ResponseCallback onSuccess,
            AgentLoop::ProviderErrorCallback onError) {
            geminiClient->generateContent(
                conversation,
                toolDeclarations,
                std::move(onSuccess),
                std::move(onError));
        },
        [database, userId = identity.userId, studentId](
            const std::string &name,
            const Json::Value &arguments,
            AgentLoop::ToolResultCallback toolResult) {
            ToolRegistry::executeAuthorized(
                database,
                name,
                arguments,
                userId,
                studentId,
                std::move(toolResult));
        },
        [responseCallback, studentId, message](
            const std::string &answer,
            const Json::Value &toolsUsed) {
            Json::Value result;
            result["student_id"] = Json::Int64(studentId);
            result["message"] = message;
            result["answer"] = answer;
            result["tools_used"] = toolsUsed;
            result["status"] = "ok";
            (*responseCallback)(
                drogon::HttpResponse::newHttpJsonResponse(result));
        },
        [responseCallback](
            const std::string &error,
            const Json::Value &toolsUsed) {
            (*responseCallback)(agentErrorResponse(
                error, toolsUsed, drogon::k502BadGateway));
        });
}
