#pragma once

#include <drogon/HttpResponse.h>

#include "ServiceResult.h"

inline drogon::HttpResponsePtr toHttpResponse(const ServiceResult &result)
{
    if (result.status == ServiceResult::Status::Ok)
    {
        return drogon::HttpResponse::newHttpJsonResponse(result.data);
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
        default:
            response->setStatusCode(drogon::k500InternalServerError);
            break;
    }
    return response;
}
