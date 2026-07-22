#pragma once

#include <drogon/orm/DbClient.h>

#include <cstdint>
#include <functional>
#include <string>

#include "ServiceResult.h"

struct CourseSearchFilters
{
    bool hasDepartment = false;
    std::string department;

    bool hasDifficulty = false;
    std::string difficulty;

    bool hasCredits = false;
    int credits = 0;

    bool hasInstructor = false;
    std::string instructor;
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
