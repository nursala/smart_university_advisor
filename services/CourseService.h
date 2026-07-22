#pragma once

#include <drogon/orm/DbClient.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "ServiceResult.h"

struct CourseSearchFilters
{
    std::optional<std::string> department;
    std::optional<std::string> difficulty;
    std::optional<int> credits;
    std::optional<std::string> instructor;
};

class CourseService
{
  public:
    static void searchCourses(
        const drogon::orm::DbClientPtr &database,
        const CourseSearchFilters &filters,
        std::function<void(ServiceResult)> &&callback);

    static void getCourseDetails(
        const drogon::orm::DbClientPtr &database,
        int64_t courseId,
        std::function<void(ServiceResult)> &&callback);
};
