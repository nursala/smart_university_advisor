#pragma once

#include <drogon/HttpController.h>

class CoursesController
    : public drogon::HttpController<CoursesController>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(CoursesController::list, "/courses", drogon::Get);
    ADD_METHOD_TO(CoursesController::details,
                  "/courses/{1}/details",
                  drogon::Get);
    METHOD_LIST_END

    void list(
        const drogon::HttpRequestPtr &request,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback) const;

    void details(
        const drogon::HttpRequestPtr &request,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback,
        int64_t courseId) const;
};
