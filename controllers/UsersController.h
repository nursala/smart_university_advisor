#pragma once

#include <drogon/HttpController.h>

class UsersController : public drogon::HttpController<UsersController>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(UsersController::me, "/users/me", drogon::Get, "JwtAuthFilter");
    ADD_METHOD_TO(UsersController::updateMe,
                  "/users/me",
                  drogon::Patch,
                  "JwtAuthFilter");
    METHOD_LIST_END

    void me(
        const drogon::HttpRequestPtr &request,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback) const;

    void updateMe(
        const drogon::HttpRequestPtr &request,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback) const;
};
