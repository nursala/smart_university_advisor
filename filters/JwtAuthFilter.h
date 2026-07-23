#pragma once

#include <drogon/HttpFilter.h>

// Attach to any route that requires a logged-in user, e.g.:
//   ADD_METHOD_TO(UsersController::me, "/users/me", drogon::Get, "JwtAuthFilter");
//
// Reads "Authorization: Bearer <token>", verifies it with JwtService, and
// on success stores the claims on the request as attributes "user_id"
// (int64_t) and "user_role" (std::string) so downstream handlers never
// re-parse the token themselves.
class JwtAuthFilter : public drogon::HttpFilter<JwtAuthFilter>
{
  public:
    void doFilter(const drogon::HttpRequestPtr &request,
                  drogon::FilterCallback &&filterCallback,
                  drogon::FilterChainCallback &&chainCallback) override;
};
