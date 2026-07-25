#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <json/json.h>

class EnrollmentConfirmationService
{
  public:
    static Json::Value propose(int64_t userId,
                               int64_t studentId,
                               int64_t courseId,
                               const std::string &semester,
                               const std::string &courseCode,
                               const std::string &courseName);
    static std::optional<Json::Value> consume(
        const std::string &confirmationId,
        int64_t userId,
        int64_t studentId,
        std::string &error);
    static std::optional<Json::Value> acquire(
        const std::string &confirmationId,
        int64_t userId,
        int64_t studentId,
        std::string &error);
    static void finalize(const std::string &confirmationId,
                         bool preserveForRetry);
};
