#pragma once

#include <drogon/HttpController.h>

class AgentController : public drogon::HttpController<AgentController>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AgentController::query, "/agent/query", drogon::Post,
                  "JwtAuthFilter");
    METHOD_LIST_END

    void query(
        const drogon::HttpRequestPtr &request,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback) const;
};
