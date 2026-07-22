#pragma once

#include <drogon/HttpResponse.h>

#include "ServiceResult.h"

inline drogon::HttpResponsePtr toHttpResponse(const ServiceResult &result)
{
    if (result.status == ServiceResult::Status::Ok ||
        result.status == ServiceResult::Status::Created)
    {
        auto response = drogon::HttpResponse::newHttpJsonResponse(result.data);
        if (result.status == ServiceResult::Status::Created)
        {
            response->setStatusCode(drogon::k201Created);
        }
        return response;
    }

    Json::Value error;
    error["error"] = result.message;
    auto response = drogon::HttpResponse::newHttpJsonResponse(error);
    switch (result.status)
    {
        case ServiceResult::Status::NotFound:
            response->setStatusCode(drogon::k404NotFound);
            break;
        case ServiceResult::Status::BadRequest:
            response->setStatusCode(drogon::k400BadRequest);
            break;
        case ServiceResult::Status::Conflict:
            response->setStatusCode(drogon::k409Conflict);
            break;
        default:
            response->setStatusCode(drogon::k500InternalServerError);
            break;
    }
    return response;
}
