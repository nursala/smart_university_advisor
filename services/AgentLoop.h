#pragma once

#include <json/json.h>

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>

class AgentLoop : public std::enable_shared_from_this<AgentLoop>
{
  public:
    using ResponseCallback = std::function<void(const Json::Value &)>;
    using ProviderErrorCallback = std::function<void(const std::string &)>;
    using RequestCallback = std::function<void(
        const Json::Value &contents,
        const Json::Value &toolDeclarations,
        ResponseCallback onSuccess,
        ProviderErrorCallback onError)>;
    using ToolResultCallback = std::function<void(Json::Value)>;
    using ExecuteToolCallback = std::function<void(
        const std::string &name,
        const Json::Value &arguments,
        ToolResultCallback callback)>;
    using CompleteCallback =
        std::function<void(const std::string &, const Json::Value &)>;
    using ErrorCallback =
        std::function<void(const std::string &, const Json::Value &)>;

    static constexpr std::size_t kDefaultMaxToolRounds = 6;

    static void start(
        Json::Value contents,
        Json::Value toolDeclarations,
        RequestCallback request,
        ExecuteToolCallback executeTool,
        CompleteCallback complete,
        ErrorCallback error,
        std::size_t maxToolRounds = kDefaultMaxToolRounds);

  private:
    AgentLoop(
        Json::Value contents,
        Json::Value toolDeclarations,
        RequestCallback request,
        ExecuteToolCallback executeTool,
        CompleteCallback complete,
        ErrorCallback error,
        std::size_t maxToolRounds);

    void requestToolRound();
    void requestFinalSynthesis();
    void handleToolRoundResponse(const Json::Value &response);
    void handleFinalSynthesisResponse(const Json::Value &response);
    // Dispatches every function call in `functionCalls` concurrently --
    // they are always independent within a single round -- and resumes
    // the loop once the last one completes. See AgentLoop.cc for the
    // ordering/synchronization contract.
    void executeFunctionCalls(
        const std::shared_ptr<Json::Value> &functionCalls);
    bool parseResponse(
        const Json::Value &response,
        Json::Value &content,
        Json::Value &functionCalls,
        std::string &text);
    bool toolAcceptsEmptyArguments(const std::string &name) const;
    void fail(const std::string &message);

    Json::Value contents_{Json::arrayValue};
    Json::Value toolDeclarations_{Json::arrayValue};
    Json::Value toolsUsed_{Json::arrayValue};
    RequestCallback request_;
    ExecuteToolCallback executeTool_;
    CompleteCallback complete_;
    ErrorCallback error_;
    std::size_t maxToolRounds_ = kDefaultMaxToolRounds;
    std::size_t toolRounds_ = 0;
    // Concurrent tool dispatch means multiple completion callbacks can
    // legitimately observe/set this from different Drogon IO threads at
    // once (see executeFunctionCalls), so a plain bool is not safe here.
    std::atomic<bool> finished_{false};
};
