#pragma once

#include <json/json.h>

#include <string>
#include <utility>

struct ServiceResult
{
    enum class Status
    {
        Ok,
        NotFound,
        BadRequest,
        Error
    };

    Status status;
    Json::Value data;
    std::string message;

    static ServiceResult ok(Json::Value resultData)
    {
        return ServiceResult{Status::Ok, std::move(resultData), ""};
    }

    static ServiceResult notFound(std::string errorMessage)
    {
        return ServiceResult{
            Status::NotFound, Json::Value(), std::move(errorMessage)};
    }

    static ServiceResult badRequest(std::string errorMessage)
    {
        return ServiceResult{
            Status::BadRequest, Json::Value(), std::move(errorMessage)};
    }

    static ServiceResult error(std::string errorMessage)
    {
        return ServiceResult{
            Status::Error, Json::Value(), std::move(errorMessage)};
    }
};
