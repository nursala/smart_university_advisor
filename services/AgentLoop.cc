#include "AgentLoop.h"

#include <utility>

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
    auto responseParts = std::make_shared<Json::Value>(Json::arrayValue);
    executeFunctionCalls(calls, 0, responseParts);
}

void AgentLoop::executeFunctionCalls(
    const std::shared_ptr<Json::Value> &functionCalls,
    Json::ArrayIndex index,
    const std::shared_ptr<Json::Value> &responseParts)
{
    if (finished_)
        return;

    if (index >= functionCalls->size())
    {
        Json::Value turn;
        turn["role"] = "user";
        turn["parts"] = *responseParts;
        contents_.append(std::move(turn));
        requestToolRound();
        return;
    }

    const auto &call = (*functionCalls)[index];
    const auto name = call["name"].asString();
    const Json::Value arguments = call["args"];

    toolsUsed_.append(name);
    auto self = shared_from_this();
    executeTool_(
        name,
        arguments,
        [self, functionCalls, index, responseParts, name](
            Json::Value toolResult) {
            if (self->finished_)
                return;
            Json::Value functionResponse;
            functionResponse["name"] = name;
            functionResponse["response"] = std::move(toolResult);
            Json::Value part;
            part["functionResponse"] = std::move(functionResponse);
            responseParts->append(std::move(part));
            self->executeFunctionCalls(
                functionCalls, index + 1, responseParts);
        });
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
