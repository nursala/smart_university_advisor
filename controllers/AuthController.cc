#include "AuthController.h"

#include <drogon/drogon.h>

#include "../services/JwtService.h"
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

bool isNonEmptyString(const Json::Value &value)
{
    return value.isString() && !value.asString().empty();
}

// Attaches a freshly issued JWT to a successful register/login
// ServiceResult and turns the combined payload into an HTTP response.
// Both endpoints share this exact response shape:
// { id, name, email, role, "token": "..." }.
drogon::HttpResponsePtr withToken(const ServiceResult &result)
{
    if (result.status != ServiceResult::Status::Ok)
    {
        return toHttpResponse(result);
    }

    try
    {
        JwtService jwtService;
        const auto userId = result.data["id"].asInt64();
        const auto role = result.data["role"].asString();

        Json::Value body = result.data;
        body["token"] = jwtService.issue(userId, role);
        return drogon::HttpResponse::newHttpJsonResponse(body);
    }
    catch (const std::exception &exception)
    {
        LOG_ERROR << "JWT is not configured: " << exception.what();
        return errorResponse("Authentication is not available",
                             drogon::k500InternalServerError);
    }
}
}  // namespace

void AuthController::registerUser(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) const
{
    const auto &body = request->getJsonObject();
    if (!body || !body->isObject() || !body->isMember("name") ||
        !body->isMember("email") || !body->isMember("password") ||
        !isNonEmptyString((*body)["name"]) ||
        !isNonEmptyString((*body)["email"]) ||
        !isNonEmptyString((*body)["password"]))
    {
        callback(errorResponse("name, email, and password are required",
                               drogon::k400BadRequest));
        return;
    }

    const auto password = (*body)["password"].asString();
    if (password.size() < 8)
    {
        callback(errorResponse("password must be at least 8 characters",
                               drogon::k400BadRequest));
        return;
    }

    // role is intentionally never read from the request body -- every
    // self-registered account is 'student'. Promoting to advisor/admin is
    // an out-of-band operation (direct DB update), not a client choice.
    UserService::registerUser(
        drogon::app().getDbClient(),
        (*body)["name"].asString(),
        (*body)["email"].asString(),
        password,
        [callback](ServiceResult result) {
            auto response = withToken(result);
            if (result.status == ServiceResult::Status::Ok)
            {
                response->setStatusCode(drogon::k201Created);
            }
            callback(response);
        });
}

void AuthController::login(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) const
{
    const auto &body = request->getJsonObject();
    if (!body || !body->isObject() || !body->isMember("email") ||
        !body->isMember("password") || !isNonEmptyString((*body)["email"]) ||
        !isNonEmptyString((*body)["password"]))
    {
        callback(errorResponse("email and password are required",
                               drogon::k400BadRequest));
        return;
    }

    UserService::login(
        drogon::app().getDbClient(),
        (*body)["email"].asString(),
        (*body)["password"].asString(),
        [callback](ServiceResult result) { callback(withToken(result)); });
}
