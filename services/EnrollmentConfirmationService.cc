#include "EnrollmentConfirmationService.h"

#include <openssl/rand.h>

#include <chrono>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>

namespace
{
struct Confirmation
{
    int64_t userId;
    int64_t studentId;
    int64_t courseId;
    std::string semester;
    std::chrono::steady_clock::time_point expires;
    bool inFlight = false;
};

std::mutex confirmationsMutex;
std::map<std::string, Confirmation> confirmations;

std::string randomId()
{
    unsigned char bytes[16];
    if (RAND_bytes(bytes, sizeof(bytes)) != 1)
        throw std::runtime_error("Unable to create confirmation identifier");
    std::ostringstream output;
    for (const auto byte : bytes)
        output << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<int>(byte);
    return output.str();
}
}  // namespace

Json::Value EnrollmentConfirmationService::propose(
    int64_t userId,
    int64_t studentId,
    int64_t courseId,
    const std::string &semester,
    const std::string &courseCode,
    const std::string &courseName)
{
    const auto id = randomId();
    {
        std::lock_guard<std::mutex> lock(confirmationsMutex);
        confirmations[id] = Confirmation{
            userId, studentId, courseId, semester,
            std::chrono::steady_clock::now() + std::chrono::minutes(5), false};
    }
    Json::Value proposal;
    proposal["confirmation_id"] = id;
    proposal["student_id"] = Json::Int64(studentId);
    proposal["course_id"] = Json::Int64(courseId);
    proposal["course_code"] = courseCode;
    proposal["course_name"] = courseName;
    proposal["semester"] = semester;
    proposal["expires_in_seconds"] = 300;
    proposal["status"] = "confirmation_required";
    return proposal;
}

std::optional<Json::Value> EnrollmentConfirmationService::consume(
    const std::string &confirmationId,
    int64_t userId,
    int64_t studentId,
    std::string &error)
{
    std::lock_guard<std::mutex> lock(confirmationsMutex);
    const auto it = confirmations.find(confirmationId);
    if (it == confirmations.end())
    {
        error = "Confirmation is invalid, expired, or already used";
        return std::nullopt;
    }
    const auto confirmation = it->second;
    if (confirmation.expires < std::chrono::steady_clock::now())
    {
        confirmations.erase(it);
        error = "Confirmation is invalid, expired, or already used";
        return std::nullopt;
    }
    if (confirmation.userId != userId ||
        confirmation.studentId != studentId)
    {
        error = "Confirmation belongs to another authenticated identity";
        return std::nullopt;
    }
    confirmations.erase(it);
    Json::Value args;
    args["student_id"] = Json::Int64(studentId);
    args["course_id"] = Json::Int64(confirmation.courseId);
    args["semester"] = confirmation.semester;
    return args;
}

std::optional<Json::Value> EnrollmentConfirmationService::acquire(
    const std::string &confirmationId,
    int64_t userId,
    int64_t studentId,
    std::string &error)
{
    std::lock_guard<std::mutex> lock(confirmationsMutex);
    const auto it = confirmations.find(confirmationId);
    if (it == confirmations.end() ||
        it->second.expires < std::chrono::steady_clock::now())
    {
        if (it != confirmations.end())
            confirmations.erase(it);
        error = "Confirmation is invalid, expired, or already used";
        return std::nullopt;
    }
    auto &confirmation = it->second;
    if (confirmation.userId != userId ||
        confirmation.studentId != studentId)
    {
        error = "Confirmation belongs to another authenticated identity";
        return std::nullopt;
    }
    if (confirmation.inFlight)
    {
        error = "Confirmation is already being processed";
        return std::nullopt;
    }
    confirmation.inFlight = true;
    Json::Value args;
    args["student_id"] = Json::Int64(studentId);
    args["course_id"] = Json::Int64(confirmation.courseId);
    args["semester"] = confirmation.semester;
    return args;
}

void EnrollmentConfirmationService::finalize(
    const std::string &confirmationId,
    bool preserveForRetry)
{
    std::lock_guard<std::mutex> lock(confirmationsMutex);
    const auto it = confirmations.find(confirmationId);
    if (it == confirmations.end())
        return;
    if (preserveForRetry &&
        it->second.expires >= std::chrono::steady_clock::now())
        it->second.inFlight = false;
    else
        confirmations.erase(it);
}
