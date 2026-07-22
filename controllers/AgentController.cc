#include "AgentController.h"

#include <memory>
#include <string>
#include <vector>

#include <drogon/drogon.h>

#include "../services/GeminiClient.h"
#include "../services/ToolRegistry.h"
#include "../services/ValidationHelpers.h"

namespace
{
// Hard cap on Gemini round trips per /agent/query call, so a model stuck
// in a tool-call loop can never hang a request indefinitely.
constexpr int kMaxAgentSteps = 6;

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

// Mutable state threaded through the async callback chain. A shared_ptr
// keeps it alive across each Gemini round trip and each tool execution,
// since none of those steps can safely capture-by-reference across an
// async boundary.
struct AgentState
{
    std::shared_ptr<GeminiClient> geminiClient;
    drogon::orm::DbClientPtr database;
    int64_t studentId = 0;
    std::string message;
    Json::Value contents{Json::arrayValue};
    Json::Value toolsUsed{Json::arrayValue};
    int step = 0;
    std::function<void(const drogon::HttpResponsePtr &)> callback;
};

void runStep(const std::shared_ptr<AgentState> &state);

void executeFunctionCalls(
    const std::shared_ptr<AgentState> &state,
    const std::shared_ptr<std::vector<Json::Value>> &functionCalls,
    size_t index,
    const std::shared_ptr<Json::Value> &responseParts)
{
    if (index >= functionCalls->size())
    {
        Json::Value turn;
        turn["role"] = "user";
        turn["parts"] = *responseParts;
        state->contents.append(turn);
        runStep(state);
        return;
    }

    const auto &call = (*functionCalls)[index];
    const auto name = call.isMember("name") ? call["name"].asString() : "";
    const Json::Value args =
        call.isMember("args") ? call["args"] : Json::Value(Json::objectValue);

    state->toolsUsed.append(name);

    ToolRegistry::execute(
        state->database,
        name,
        args,
        [state, functionCalls, index, responseParts, name](
            Json::Value toolResult) {
            Json::Value functionResponse;
            functionResponse["name"] = name;
            functionResponse["response"] = std::move(toolResult);
            Json::Value part;
            part["functionResponse"] = std::move(functionResponse);
            responseParts->append(std::move(part));
            executeFunctionCalls(state, functionCalls, index + 1, responseParts);
        });
}

void handleGeminiResponse(const std::shared_ptr<AgentState> &state,
                          const Json::Value &response)
{
    if (!response.isMember("candidates") || !response["candidates"].isArray() ||
        response["candidates"].empty())
    {
        state->callback(agentErrorResponse(
            "Gemini API returned no candidates (the request may have been "
            "blocked)",
            state->toolsUsed,
            drogon::k502BadGateway));
        return;
    }

    const auto &candidate = response["candidates"][0];
    const auto &content = candidate["content"];

    auto functionCalls = std::make_shared<std::vector<Json::Value>>();
    std::string finalText;

    if (content.isObject() && content.isMember("parts") &&
        content["parts"].isArray())
    {
        for (const auto &part : content["parts"])
        {
            if (part.isMember("functionCall"))
            {
                functionCalls->push_back(part["functionCall"]);
            }
            if (part.isMember("text") && part["text"].isString())
            {
                finalText += part["text"].asString();
            }
        }
    }

    if (functionCalls->empty())
    {
        Json::Value result;
        result["student_id"] = Json::Int64(state->studentId);
        result["message"] = state->message;
        result["answer"] = finalText;
        result["tools_used"] = state->toolsUsed;
        result["status"] = "ok";
        state->callback(drogon::HttpResponse::newHttpJsonResponse(result));
        return;
    }

    // Preserve the model's turn verbatim (including fields like
    // thoughtSignature/id that Gemini 3.x attaches to functionCall parts
    // and expects echoed back on the next turn).
    state->contents.append(content);

    auto responseParts = std::make_shared<Json::Value>(Json::arrayValue);
    executeFunctionCalls(state, functionCalls, 0, responseParts);
}

void runStep(const std::shared_ptr<AgentState> &state)
{
    if (state->step >= kMaxAgentSteps)
    {
        state->callback(agentErrorResponse(
            "Agent reached the step limit without producing a final answer",
            state->toolsUsed,
            drogon::k502BadGateway));
        return;
    }
    state->step += 1;

    state->geminiClient->generateContent(
        state->contents,
        ToolRegistry::toolDeclarations(),
        [state](const Json::Value &response) {
            handleGeminiResponse(state, response);
        },
        [state](const std::string &message) {
            state->callback(agentErrorResponse(
                message, state->toolsUsed, drogon::k502BadGateway));
        });
}
}  // namespace

void AgentController::query(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) const
{
    const auto &body = request->getJsonObject();
    int64_t studentId = 0;
    std::string parseError;
    if (!body || !body->isObject() || !body->isMember("student_id") ||
        !ValidationHelpers::tryGetInt64(
            (*body)["student_id"], studentId, parseError) ||
        studentId <= 0 || !body->isMember("message") ||
        !(*body)["message"].isString() || (*body)["message"].asString().empty())
    {
        callback(errorResponse(
            "student_id (a positive integer) and a non-empty message are "
            "required",
            drogon::k400BadRequest));
        return;
    }

    const auto message = (*body)["message"].asString();

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

    auto state = std::make_shared<AgentState>();
    state->geminiClient = std::move(geminiClient);
    state->database = drogon::app().getDbClient();
    state->studentId = studentId;
    state->message = message;
    state->callback = std::move(callback);

    Json::Value textPart;
    textPart["text"] =
        "You are an academic advisor assistant. You are helping student_id " +
        std::to_string(studentId) +
        ". Always pass student_id=" + std::to_string(studentId) +
        " when calling tools that require a student_id. Use the available "
        "tools as needed to answer accurately, then give a clear, concise "
        "final answer in plain text.\n\nStudent's question: " + message;
    Json::Value parts(Json::arrayValue);
    parts.append(std::move(textPart));
    Json::Value userTurn;
    userTurn["role"] = "user";
    userTurn["parts"] = std::move(parts);
    state->contents.append(std::move(userTurn));

    runStep(state);
}
