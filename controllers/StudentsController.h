#pragma once

#include <drogon/HttpController.h>

class StudentsController
    : public drogon::HttpController<StudentsController>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(StudentsController::profile,
                  "/students/{1}/profile",
                  drogon::Get, "JwtAuthFilter");
    ADD_METHOD_TO(StudentsController::academicSummary,
                  "/students/{1}/academic-summary",
                  drogon::Get, "JwtAuthFilter");
    ADD_METHOD_TO(StudentsController::availableCourses,
                  "/students/{1}/available-courses",
                  drogon::Get, "JwtAuthFilter");
    ADD_METHOD_TO(StudentsController::courseRecommendations,
                  "/students/{1}/course-recommendations",
                  drogon::Post, "JwtAuthFilter");
    ADD_METHOD_TO(StudentsController::semesterPlan,
                  "/students/{1}/semester-plan",
                  drogon::Post, "JwtAuthFilter");
    ADD_METHOD_TO(StudentsController::riskAnalysis,
                  "/students/{1}/risk-analysis",
                  drogon::Post, "JwtAuthFilter");
    METHOD_LIST_END

    void profile(
        const drogon::HttpRequestPtr &request,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback,
        int64_t studentId) const;

    void academicSummary(
        const drogon::HttpRequestPtr &request,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback,
        int64_t studentId) const;

    void availableCourses(
        const drogon::HttpRequestPtr &request,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback,
        int64_t studentId) const;

    void courseRecommendations(
        const drogon::HttpRequestPtr &request,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback,
        int64_t studentId) const;

    void semesterPlan(
        const drogon::HttpRequestPtr &request,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback,
        int64_t studentId) const;

    void riskAnalysis(
        const drogon::HttpRequestPtr &request,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback,
        int64_t studentId) const;
};
