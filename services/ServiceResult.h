#pragma once

#include <json/json.h>

#include <string>
#include <utility>

struct ServiceResult
{
    enum class Status
    {
        Ok,
        Created,
        NotFound,
        BadRequest,
        Conflict,
        Error
    };

    Status status;
    Json::Value data;
    std::string message;

    static ServiceResult ok(Json::Value resultData)
    {
        return ServiceResult{Status::Ok, std::move(resultData), ""};
    }

    static ServiceResult created(Json::Value resultData)
    {
        return ServiceResult{Status::Created, std::move(resultData), ""};
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

    static ServiceResult conflict(std::string errorMessage)
    {
        return ServiceResult{
            Status::Conflict, Json::Value(), std::move(errorMessage)};
    }

    static ServiceResult error(std::string errorMessage)
    {
        return ServiceResult{
            Status::Error, Json::Value(), std::move(errorMessage)};
    }
};
