#pragma once

#include <drogon/orm/DbClient.h>

#include <cstdint>
#include <functional>
#include <string>

#include "ServiceResult.h"

class EnrollmentService
{
  public:
    static void create(
        const drogon::orm::DbClientPtr &database,
        int64_t studentId,
        int64_t courseId,
        const std::string &semester,
        std::function<void(ServiceResult)> &&callback);

    static void recordGrade(
        const drogon::orm::DbClientPtr &database,
        int64_t enrollmentId,
        double grade,
        std::function<void(ServiceResult)> &&callback);

    static void remove(
        const drogon::orm::DbClientPtr &database,
        int64_t enrollmentId,
        std::function<void(ServiceResult)> &&callback);
};
