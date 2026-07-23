#include "UsersController.h"

#include <drogon/drogon.h>

#include "../services/ServiceResultHttp.h"
#include "../services/UserService.h"

namespace
{
drogon::HttpResponsePtr errorResponse(const std::string &message,
                                      drogon::HttpStatusCode statusCode)
{
    Json::Value error;
    error["error"] = message;
    auto response = drogon::HttpResponse::newHttpJsonResponse(error);
    response->setStatusCode(statusCode);
    return response;
}
}  // namespace

void UsersController::me(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) const
{
    // JwtAuthFilter has already verified the token and attached user_id.
    const auto userId = request->attributes()->get<int64_t>("user_id");
    UserService::getProfile(
        drogon::app().getDbClient(), userId, [callback](ServiceResult result) {
            callback(toHttpResponse(result));
        });
}

void UsersController::updateMe(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) const
{
    const auto userId = request->attributes()->get<int64_t>("user_id");

    const auto &body = request->getJsonObject();
    if (!body || !body->isObject())
    {
        callback(errorResponse("A JSON body is required", drogon::k400BadRequest));
        return;
    }

    bool hasName = false;
    std::string name;
    if (body->isMember("name") && !(*body)["name"].isNull())
    {
        if (!(*body)["name"].isString() || (*body)["name"].asString().empty())
        {
            callback(errorResponse("name must be a non-empty string",
                                   drogon::k400BadRequest));
            return;
        }
        hasName = true;
        name = (*body)["name"].asString();
    }

    bool hasEmail = false;
    std::string email;
    if (body->isMember("email") && !(*body)["email"].isNull())
    {
        if (!(*body)["email"].isString() || (*body)["email"].asString().empty())
        {
            callback(errorResponse("email must be a non-empty string",
                                   drogon::k400BadRequest));
            return;
        }
        hasEmail = true;
        email = (*body)["email"].asString();
    }

    UserService::updateProfile(
        drogon::app().getDbClient(),
        userId,
        hasName,
        name,
        hasEmail,
        email,
        [callback](ServiceResult result) { callback(toHttpResponse(result)); });
}
