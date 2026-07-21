#pragma once

#include <drogon/HttpController.h>

class EnrollmentsController
    : public drogon::HttpController<EnrollmentsController>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(EnrollmentsController::create, "/enrollments", drogon::Post);
    ADD_METHOD_TO(EnrollmentsController::recordGrade,
                  "/enrollments/{1}/grade",
                  drogon::Patch);
    ADD_METHOD_TO(EnrollmentsController::remove,
                  "/enrollments/{1}",
                  drogon::Delete);
    METHOD_LIST_END

    void create(
        const drogon::HttpRequestPtr &request,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback) const;

    void recordGrade(
        const drogon::HttpRequestPtr &request,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback,
        int64_t enrollmentId) const;

    void remove(
        const drogon::HttpRequestPtr &request,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback,
        int64_t enrollmentId) const;
};
