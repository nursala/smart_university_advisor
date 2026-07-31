#include "AgentLoop.h"

#include <atomic>
#include <utility>
#include <vector>

namespace
{
constexpr const char *kNoCandidatesError =
    "Gemini API returned no candidates (the request may have been blocked)";
constexpr const char *kMalformedResponseError =
    "Gemini API returned a malformed response";
}

void AgentLoop::start(
    Json::Value contents,
    Json::Value toolDeclarations,
    RequestCallback request,
    ExecuteToolCallback executeTool,
    CompleteCallback complete,
    ErrorCallback error,
    std::size_t maxToolRounds)
{
    auto loop = std::shared_ptr<AgentLoop>(new AgentLoop(
        std::move(contents),
        std::move(toolDeclarations),
        std::move(request),
        std::move(executeTool),
        std::move(complete),
        std::move(error),
        maxToolRounds));
    loop->requestToolRound();
}

AgentLoop::AgentLoop(
    Json::Value contents,
    Json::Value toolDeclarations,
    RequestCallback request,
    ExecuteToolCallback executeTool,
    CompleteCallback complete,
    ErrorCallback error,
    std::size_t maxToolRounds)
    : contents_(std::move(contents)),
      toolDeclarations_(std::move(toolDeclarations)),
      request_(std::move(request)),
      executeTool_(std::move(executeTool)),
      complete_(std::move(complete)),
      error_(std::move(error)),
      maxToolRounds_(maxToolRounds)
{
}

void AgentLoop::requestToolRound()
{
    if (finished_)
        return;

    if (toolRounds_ >= maxToolRounds_)
    {
        requestFinalSynthesis();
        return;
    }

    auto self = shared_from_this();
    request_(
        contents_,
        toolDeclarations_,
        [self](const Json::Value &response) {
            try
            {
                self->handleToolRoundResponse(response);
            }
            catch (...)
            {
                self->fail(kMalformedResponseError);
            }
        },
        [self](const std::string &message) {
            self->fail(message);
        });
}

void AgentLoop::requestFinalSynthesis()
{
    if (finished_)
        return;

    Json::Value instruction;
    instruction["text"] =
        "The tool-round limit has been reached. Give the final answer now "
        "using only the conversation and tool results already provided. Do "
        "not request or imply that any additional tool was called.";
    Json::Value parts(Json::arrayValue);
    parts.append(std::move(instruction));
    Json::Value turn;
    turn["role"] = "user";
    turn["parts"] = std::move(parts);
    contents_.append(std::move(turn));

    auto self = shared_from_this();
    request_(
        contents_,
        Json::Value(Json::arrayValue),
        [self](const Json::Value &response) {
            try
            {
                self->handleFinalSynthesisResponse(response);
            }
            catch (...)
            {
                self->fail(kMalformedResponseError);
            }
        },
        [self](const std::string &message) {
            self->fail(message);
        });
}

bool AgentLoop::parseResponse(
    const Json::Value &response,
    Json::Value &content,
    Json::Value &functionCalls,
    std::string &text)
{
    if (!response.isObject())
    {
        fail(kMalformedResponseError);
        return false;
    }

    if (!response.isMember("candidates") ||
        !response["candidates"].isArray())
    {
        fail(kMalformedResponseError);
        return false;
    }
    if (response["candidates"].empty())
    {
        fail(kNoCandidatesError);
        return false;
    }

    for (const auto &candidate : response["candidates"])
    {
        if (!candidate.isObject() || !candidate.isMember("content") ||
            !candidate["content"].isObject())
        {
            continue;
        }
        const auto &candidateContent = candidate["content"];
        if (!candidateContent.isMember("parts") ||
            !candidateContent["parts"].isArray())
        {
            continue;
        }
        Json::Value candidateFunctionCalls(Json::arrayValue);
        std::string candidateText;
        bool candidateIsValid = true;
        for (const auto &part : candidateContent["parts"])
        {
            if (!part.isObject())
            {
                candidateIsValid = false;
                break;
            }

            if (part.isMember("text"))
            {
                if (!part["text"].isString())
                {
                    candidateIsValid = false;
                    break;
                }
                candidateText += part["text"].asString();
            }

            if (part.isMember("functionCall"))
            {
                const auto &functionCall = part["functionCall"];
                if (!functionCall.isObject() ||
                    !functionCall.isMember("name") ||
                    !functionCall["name"].isString())
                {
                    candidateIsValid = false;
                    break;
                }

                const auto name = functionCall["name"].asString();
                if (name.empty() ||
                    name.find_first_not_of(" \t\r\n") ==
                        std::string::npos)
                {
                    candidateIsValid = false;
                    break;
                }

                Json::Value validatedCall = functionCall;
                if (functionCall.isMember("args"))
                {
                    if (!functionCall["args"].isObject())
                    {
                        candidateIsValid = false;
                        break;
                    }
                }
                else
                {
                    if (!toolAcceptsEmptyArguments(name))
                    {
                        candidateIsValid = false;
                        break;
                    }
                    validatedCall["args"] =
                        Json::Value(Json::objectValue);
                }
                candidateFunctionCalls.append(
                    std::move(validatedCall));
            }
        }

        if (!candidateIsValid)
            continue;

        content = candidateContent;
        functionCalls = std::move(candidateFunctionCalls);
        text = std::move(candidateText);
        return true;
    }

    fail(kMalformedResponseError);
    return false;
}

bool AgentLoop::toolAcceptsEmptyArguments(const std::string &name) const
{
    if (!toolDeclarations_.isArray())
        return false;

    for (const auto &declaration : toolDeclarations_)
    {
        if (!declaration.isObject() || !declaration.isMember("name") ||
            !declaration["name"].isString() ||
            declaration["name"].asString() != name ||
            !declaration.isMember("parameters") ||
            !declaration["parameters"].isObject())
        {
            continue;
        }

        const auto &parameters = declaration["parameters"];
        return !parameters.isMember("required") ||
               (parameters["required"].isArray() &&
                parameters["required"].empty());
    }
    return false;
}

void AgentLoop::handleToolRoundResponse(const Json::Value &response)
{
    if (finished_)
        return;

    Json::Value content;
    Json::Value functionCalls;
    std::string finalText;
    if (!parseResponse(response, content, functionCalls, finalText))
        return;

    if (functionCalls.empty())
    {
        finished_ = true;
        complete_(finalText, toolsUsed_);
        return;
    }

    ++toolRounds_;
    contents_.append(content);
    auto calls = std::make_shared<Json::Value>(std::move(functionCalls));
    executeFunctionCalls(calls);
}

void AgentLoop::executeFunctionCalls(
    const std::shared_ptr<Json::Value> &functionCalls)
{
    if (finished_)
        return;

    const Json::ArrayIndex callCount = functionCalls->size();

    // Every call within one round is independent -- Gemini never asks for
    // a second tool whose arguments depend on a sibling call's result in
    // the same round -- so all of them are dispatched immediately instead
    // of waiting for each callback before starting the next. Drogon's
    // DbClient is safe to call concurrently, and its async callbacks can
    // legitimately land on different IO threads, so completion tracking
    // and the shared response buffer both need real synchronization
    // rather than the single-flight assumption the old recursive version
    // relied on.
    auto responseSlots =
        std::make_shared<std::vector<Json::Value>>(callCount);
    auto remaining =
        std::make_shared<std::atomic<Json::ArrayIndex>>(callCount);

    for (Json::ArrayIndex index = 0; index < callCount; ++index)
    {
        const auto &call = (*functionCalls)[index];
        const auto name = call["name"].asString();
        const Json::Value arguments = call["args"];

        toolsUsed_.append(name);
        auto self = shared_from_this();
        executeTool_(
            name,
            arguments,
            [self, responseSlots, remaining, index, name](
                Json::Value toolResult) {
                if (self->finished_)
                    return;

                Json::Value functionResponse;
                functionResponse["name"] = name;
                functionResponse["response"] = std::move(toolResult);
                Json::Value part;
                part["functionResponse"] = std::move(functionResponse);
                // Each callback writes only its own reserved slot, so
                // this is race-free without a mutex; the atomic
                // decrement below is what safely picks exactly one
                // callback -- whichever happens to finish last -- to be
                // the one that resumes the loop.
                (*responseSlots)[index] = std::move(part);

                if (--(*remaining) != 0)
                    return;

                Json::Value responseParts(Json::arrayValue);
                for (auto &slot : *responseSlots)
                {
                    responseParts.append(std::move(slot));
                }
                Json::Value turn;
                turn["role"] = "user";
                turn["parts"] = std::move(responseParts);
                self->contents_.append(std::move(turn));
                self->requestToolRound();
            });
    }
}

void AgentLoop::handleFinalSynthesisResponse(const Json::Value &response)
{
    if (finished_)
        return;

    Json::Value content;
    Json::Value functionCalls;
    std::string finalText;
    if (!parseResponse(response, content, functionCalls, finalText))
        return;

    if (!functionCalls.empty())
    {
        fail(
            "Gemini requested another tool after the tool-round limit; "
            "no final answer was produced");
        return;
    }
    if (finalText.empty())
    {
        fail("Gemini returned an empty final answer");
        return;
    }

    finished_ = true;
    complete_(finalText, toolsUsed_);
}

void AgentLoop::fail(const std::string &message)
{
    if (finished_)
        return;
    finished_ = true;
    error_(message, toolsUsed_);
}
