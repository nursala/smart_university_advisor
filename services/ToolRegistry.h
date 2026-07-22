#pragma once

#include <drogon/orm/DbClient.h>
#include <json/json.h>

#include <functional>
#include <string>
#include <vector>

// The 8 function tools exposed to the Gemini agentic loop. Each tool
// wraps the SAME service-layer logic already used by the corresponding
// REST endpoint (StudentService / CourseService) -- no DB query or
// business rule is reimplemented here.
class ToolRegistry
{
  public:
    // Gemini's `functionDeclarations` array (name/description/parameters
    // per tool), in a fixed, stable order.
    static Json::Value toolDeclarations();

    // Executes `toolName` with `args` (the JSON object Gemini sent as
    // functionCall.args). Always invokes `callback` with a JSON object
    // shaped either {"success": true, "data": ...} or
    // {"success": false, "error": "..."}; never throws for expected
    // "not found"/validation failures, and any unexpected failure from
    // the service layer is already caught and surfaced the same way, so
    // a single failing tool call never crashes the agent loop.
    static void execute(
        const drogon::orm::DbClientPtr &database,
        const std::string &toolName,
        const Json::Value &args,
        std::function<void(Json::Value)> &&callback);
};
