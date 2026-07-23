#include "JwtAuthFilter.h"

#include <drogon/drogon.h>

#include <optional>
#include <string_view>

#include "../services/JwtService.h"

namespace
{
constexpr std::string_view kBearerPrefix = "Bearer ";

drogon::HttpResponsePtr unauthorized(const std::string &message)
{
    Json::Value error;
    error["error"] = message;
    auto response = drogon::HttpResponse::newHttpJsonResponse(error);
    response->setStatusCode(drogon::k401Unauthorized);
    return response;
}
}  // namespace

void JwtAuthFilter::doFilter(const drogon::HttpRequestPtr &request,
                             drogon::FilterCallback &&filterCallback,
                             drogon::FilterChainCallback &&chainCallback)
{
    const auto &authHeader = request->getHeader("Authorization");
    if (authHeader.size() <= kBearerPrefix.size() ||
        authHeader.compare(0, kBearerPrefix.size(), kBearerPrefix) != 0)
    {
        filterCallback(
            unauthorized("Missing or malformed Authorization header"));
        return;
    }

    const auto token = authHeader.substr(kBearerPrefix.size());

    std::optional<JwtService::Claims> claims;
    try
    {
        claims = JwtService().verify(token);
    }
    catch (const std::exception &exception)
    {
        LOG_ERROR << "JWT is not configured: " << exception.what();
        Json::Value error;
        error["error"] = "Authentication is not available";
        auto response = drogon::HttpResponse::newHttpJsonResponse(error);
        response->setStatusCode(drogon::k500InternalServerError);
        filterCallback(response);
        return;
    }

    if (!claims)
    {
        filterCallback(unauthorized("Invalid or expired token"));
        return;
    }

    request->attributes()->insert("user_id", claims->userId);
    request->attributes()->insert("user_role", claims->role);
    chainCallback();
}
