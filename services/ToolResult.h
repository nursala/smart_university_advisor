#pragma once

#include <json/json.h>

#include <string>

#include "ServiceResult.h"

inline Json::Value toToolResult(const ServiceResult &result)
{
    Json::Value toolResult;
    if (result.status == ServiceResult::Status::Ok ||
        result.status == ServiceResult::Status::Created)
    {
        toolResult["success"] = true;
        toolResult["data"] = result.data;
    }
    else
    {
        toolResult["success"] = false;
        toolResult["error"] = result.message;
    }
    return toolResult;
}

inline Json::Value toolFailure(const std::string &message)
{
    Json::Value toolResult;
    toolResult["success"] = false;
    toolResult["error"] = message;
    return toolResult;
}
