#pragma once

#include <drogon/orm/DbClient.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "ServiceResult.h"

class StudentService
{
  public:
    static bool isValidDifficulty(const std::string &difficulty);

    static void getProfile(
        const drogon::orm::DbClientPtr &database,
        int64_t studentId,
        std::function<void(ServiceResult)> &&callback);

    static void getAcademicSummary(
        const drogon::orm::DbClientPtr &database,
        int64_t studentId,
        std::function<void(ServiceResult)> &&callback);

    static void getAvailableCourses(
        const drogon::orm::DbClientPtr &database,
        int64_t studentId,
        std::function<void(ServiceResult)> &&callback);

    static void getCourseRecommendations(
        const drogon::orm::DbClientPtr &database,
        int64_t studentId,
        const std::optional<std::string> &preferredDifficulty,
        int64_t maxRecommendations,
        std::function<void(ServiceResult)> &&callback);

    static void buildSemesterPlan(
        const drogon::orm::DbClientPtr &database,
        int64_t studentId,
        const std::optional<std::string> &preferredDifficulty,
        const std::optional<int64_t> &maxCredits,
        std::function<void(ServiceResult)> &&callback);

    static void analyzeRisk(
        const drogon::orm::DbClientPtr &database,
        int64_t studentId,
        const std::vector<int64_t> &courseIds,
        std::function<void(ServiceResult)> &&callback);

    // Shared eligibility query (also used by CoursesController-adjacent
    // tools). Exposed so callers needing the raw available-course rows
    // (id, code, name, department, credits, difficulty_level,
    // estimated_weekly_hours) don't re-implement the eligibility SQL.
    static void fetchAvailableCourses(
        const drogon::orm::DbClientPtr &database,
        int64_t studentId,
        std::function<void(const drogon::orm::Result &)> &&onSuccess,
        std::function<void(const drogon::orm::DrogonDbException &)>
            &&onError);
};
