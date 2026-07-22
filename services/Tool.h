#pragma once

#include <drogon/orm/DbClient.h>
#include <json/json.h>

#include <functional>

// Abstract base for one agent function-tool. Each concrete Tool wraps the
// same service-layer call the corresponding REST endpoint uses; no
// business logic lives here or in ToolRegistry.
class Tool
{
  public:
    virtual ~Tool() = default;

    // This tool's Gemini `functionDeclarations` entry (name/description/
    // parameters). ToolRegistry uses declaration()["name"] as the lookup
    // key, so it must be unique and stable.
    virtual Json::Value declaration() const = 0;

    // Executes the tool with `args` (the JSON object Gemini sent as
    // functionCall.args). Always invokes `callback` with a JSON object
    // shaped either {"success": true, "data": ...} or
    // {"success": false, "error": "..."}.
    virtual void execute(
        const drogon::orm::DbClientPtr &database,
        const Json::Value &args,
        std::function<void(Json::Value)> &&callback) const = 0;
};
