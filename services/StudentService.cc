#include "StudentService.h"

#include <algorithm>
#include <cmath>
#include <map>

#include <drogon/drogon.h>

#include "JsonHelpers.h"

namespace
{
struct AvailableCourse
{
    int64_t id;
    std::string code;
    std::string name;
    std::string department;
    int credits;
    std::string difficultyLevel;
    int estimatedWeeklyHours;
};

std::vector<AvailableCourse> toAvailableCourses(
    const drogon::orm::Result &courses)
{
    std::vector<AvailableCourse> result;
    result.reserve(courses.size());
    for (const auto &row : courses)
    {
        AvailableCourse course;
        course.id = row["id"].as<int64_t>();
        course.code = row["code"].as<std::string>();
        course.name = row["name"].as<std::string>();
        course.department = row["department"].as<std::string>();
        course.credits = row["credits"].as<int>();
        course.difficultyLevel = row["difficulty_level"].as<std::string>();
        course.estimatedWeeklyHours =
            row["estimated_weekly_hours"].as<int>();
        result.push_back(std::move(course));
    }
    return result;
}
}  // namespace

bool StudentService::isValidDifficulty(const std::string &difficulty)
{
    return difficulty == "easy" || difficulty == "medium" ||
           difficulty == "hard";
}

void StudentService::fetchAvailableCourses(
    const drogon::orm::DbClientPtr &database,
    int64_t studentId,
    std::function<void(const drogon::orm::Result &)> &&onSuccess,
    std::function<void(const drogon::orm::DrogonDbException &)> &&onError)
{
    database->execSqlAsync(
        "SELECT c.id, c.code, c.name, c.department, c.credits, "
        "c.difficulty_level, c.estimated_weekly_hours "
        "FROM courses c "
        "WHERE NOT EXISTS ("
        "SELECT 1 FROM enrollments e "
        "WHERE e.student_id = $1 AND e.course_id = c.id "
        "AND (e.status IN ('planned', 'active') "
        "OR (e.status='completed' AND EXISTS ("
        " SELECT 1 FROM grades ag WHERE ag.enrollment_id=e.id "
        " AND ag.passed=TRUE)))) "
        "AND NOT EXISTS ("
        "SELECT 1 FROM course_prerequisites cp "
        "WHERE cp.course_id = c.id "
        "AND NOT EXISTS ("
        "SELECT 1 FROM enrollments e "
        "JOIN grades g ON g.enrollment_id = e.id "
        "WHERE e.student_id = $2 "
        "AND e.course_id = cp.prerequisite_course_id "
        "AND e.status = 'completed' "
        "AND g.passed=TRUE AND g.grade >= cp.minimum_grade)) "
        "ORDER BY c.id",
        std::move(onSuccess),
        std::move(onError),
        studentId,
        studentId);
}

void StudentService::getProfile(
    const drogon::orm::DbClientPtr &database,
    int64_t studentId,
    std::function<void(ServiceResult)> &&callback)
{
    database->execSqlAsync(
        "SELECT s.id, s.student_number, s.department, s.year_level, "
        "s.current_gpa, s.max_weekly_credits, u.name, u.email "
        "FROM students s "
        "JOIN users u ON u.id = s.user_id "
        "WHERE s.id = $1",
        [callback](const drogon::orm::Result &result) {
            if (result.empty())
            {
                callback(ServiceResult::notFound("Student not found"));
                return;
            }

            const auto &row = result.front();
            Json::Value student;
            student["id"] = Json::Int64(row["id"].as<int64_t>());
            student["student_number"] =
                row["student_number"].as<std::string>();
            student["department"] = row["department"].as<std::string>();
            student["year_level"] = row["year_level"].as<int>();
            student["current_gpa"] = row["current_gpa"].isNull()
                                         ? Json::Value(Json::nullValue)
                                         : Json::Value(
                                               std::round(
                                                   row["current_gpa"].as<double>() *
                                                   100.0) /
                                               100.0);
            student["max_weekly_credits"] =
                row["max_weekly_credits"].as<int>();
            student["name"] = row["name"].as<std::string>();
            student["email"] = row["email"].as<std::string>();
            callback(ServiceResult::ok(std::move(student)));
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to load student profile: "
                      << exception.base().what();
            callback(ServiceResult::error("Unable to load student profile"));
        },
        studentId);
}

void StudentService::getAcademicSummary(
    const drogon::orm::DbClientPtr &database,
    int64_t studentId,
    std::function<void(ServiceResult)> &&callback)
{
    database->execSqlAsync(
        "SELECT s.current_gpa, e.id AS enrollment_id, e.course_id, "
        "c.code AS course_code, c.name AS course_name, e.semester, "
        "c.credits, e.status, g.grade, g.passed "
        "FROM students s "
        "LEFT JOIN enrollments e ON e.student_id = s.id "
        "LEFT JOIN grades g ON g.enrollment_id = e.id "
        "LEFT JOIN courses c ON c.id = e.course_id "
        "WHERE s.id = $1 "
        "ORDER BY e.id",
        [callback](const drogon::orm::Result &result) {
            if (result.empty())
            {
                callback(ServiceResult::notFound("Student not found"));
                return;
            }

            Json::Value summary;
            summary["current_gpa"] = result.front()["current_gpa"].isNull()
                                         ? Json::Value(Json::nullValue)
                                         : Json::Value(
                                               std::round(
                                                   result.front()["current_gpa"]
                                                       .as<double>() *
                                                   100.0) /
                                               100.0);
            Json::Value completed(Json::arrayValue);
            Json::Value active(Json::arrayValue);
            Json::Value planned(Json::arrayValue);
            int64_t completedCount = 0;
            int64_t activeCount = 0;
            int64_t plannedCount = 0;
            int64_t failedCount = 0;
            int64_t completedCredits = 0;

            for (const auto &row : result)
            {
                if (row["enrollment_id"].isNull())
                    continue;

                Json::Value course;
                course["enrollment_id"] =
                    Json::Int64(row["enrollment_id"].as<int64_t>());
                course["course_id"] =
                    Json::Int64(row["course_id"].as<int64_t>());
                course["course_code"] =
                    row["course_code"].as<std::string>();
                course["course_name"] =
                    row["course_name"].as<std::string>();
                course["semester"] = row["semester"].as<std::string>();
                course["credits"] = row["credits"].as<int>();
                const auto status = row["status"].as<std::string>();
                course["status"] = status;

                if (status == "planned")
                {
                    ++plannedCount;
                    planned.append(std::move(course));
                }
                else if (status == "active")
                {
                    ++activeCount;
                    active.append(std::move(course));
                }
                else if (status == "completed")
                {
                    course["grade"] = row["grade"].isNull()
                                          ? Json::Value(Json::nullValue)
                                          : Json::Value(
                                                row["grade"].as<double>());
                    course["passed"] = row["passed"].isNull()
                                           ? Json::Value(Json::nullValue)
                                           : Json::Value(
                                                 row["passed"].as<bool>());
                    if (!row["passed"].isNull() &&
                        row["passed"].as<bool>())
                    {
                        ++completedCount;
                        completedCredits += row["credits"].as<int64_t>();
                    }
                    else if (!row["passed"].isNull())
                    {
                        ++failedCount;
                    }
                    completed.append(std::move(course));
                }
            }

            summary["completed_courses_count"] =
                Json::Int64(completedCount);
            summary["active_courses_count"] = Json::Int64(activeCount);
            summary["planned_courses_count"] = Json::Int64(plannedCount);
            summary["failed_courses_count"] = Json::Int64(failedCount);
            summary["completed_credits"] = Json::Int64(completedCredits);
            summary["completed_courses"] = std::move(completed);
            summary["active_courses"] = std::move(active);
            summary["planned_courses"] = std::move(planned);
            callback(ServiceResult::ok(std::move(summary)));
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to load academic summary: "
                      << exception.base().what();
            callback(
                ServiceResult::error("Unable to load academic summary"));
        },
        studentId);
}

void StudentService::getAvailableCourses(
    const drogon::orm::DbClientPtr &database,
    int64_t studentId,
    std::function<void(ServiceResult)> &&callback)
{
    database->execSqlAsync(
        "SELECT id FROM students WHERE id = $1",
        [database, callback, studentId](const drogon::orm::Result &students) {
            if (students.empty())
            {
                callback(ServiceResult::notFound("Student not found"));
                return;
            }

            fetchAvailableCourses(
                database,
                studentId,
                [callback](const drogon::orm::Result &courses) {
                    callback(ServiceResult::ok(toJsonArray(
                        courses,
                        [](const drogon::orm::Row &row) {
                            Json::Value course;
                            course["id"] = Json::Int64(row["id"].as<int64_t>());
                            course["code"] = row["code"].as<std::string>();
                            course["name"] = row["name"].as<std::string>();
                            course["department"] =
                                row["department"].as<std::string>();
                            course["credits"] = row["credits"].as<int>();
                            course["difficulty_level"] =
                                row["difficulty_level"].as<std::string>();
                            return course;
                        })));
                },
                [callback](const drogon::orm::DrogonDbException &exception) {
                    LOG_ERROR << "Failed to load available courses: "
                              << exception.base().what();
                    callback(ServiceResult::error(
                        "Unable to load available courses"));
                });
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to validate student: "
                      << exception.base().what();
            callback(ServiceResult::error("Unable to load available courses"));
        },
        studentId);
}

void StudentService::getCourseRecommendations(
    const drogon::orm::DbClientPtr &database,
    int64_t studentId,
    const std::optional<std::string> &preferredDifficulty,
    int64_t maxRecommendations,
    std::function<void(ServiceResult)> &&callback)
{
    database->execSqlAsync(
        "SELECT id, current_gpa FROM students WHERE id = $1",
        [database,
         callback,
         studentId,
         preferredDifficulty,
         maxRecommendations](const drogon::orm::Result &students) {
            if (students.empty())
            {
                callback(ServiceResult::notFound("Student not found"));
                return;
            }

            const std::optional<double> currentGpa =
                students.front()["current_gpa"].isNull()
                    ? std::nullopt
                    : std::optional<double>(
                          students.front()["current_gpa"].as<double>());

            fetchAvailableCourses(
                database,
                studentId,
                [callback,
                 studentId,
                 currentGpa,
                 preferredDifficulty,
                 maxRecommendations](const drogon::orm::Result &courses) {
                    auto availableCourses = toAvailableCourses(courses);

                    std::vector<int> scores(availableCourses.size(), 0);
                    for (size_t i = 0; i < availableCourses.size(); ++i)
                    {
                        const auto &course = availableCourses[i];
                        int score = 0;
                        if (preferredDifficulty.has_value() &&
                            course.difficultyLevel == preferredDifficulty.value())
                        {
                            score += 30;
                        }

                        if (!currentGpa.has_value())
                        {
                            if (course.difficultyLevel == "easy")
                                score += 20;
                            else if (course.difficultyLevel == "medium")
                                score += 15;
                            else
                                score += 10;
                        }
                        else if (*currentGpa < 70)
                        {
                            if (course.difficultyLevel == "easy")
                                score += 25;
                            else if (course.difficultyLevel == "medium")
                                score += 15;
                        }
                        else if (*currentGpa <= 85)
                        {
                            if (course.difficultyLevel == "easy")
                                score += 10;
                            else if (course.difficultyLevel == "medium")
                                score += 25;
                            else if (course.difficultyLevel == "hard")
                                score += 15;
                        }
                        else
                        {
                            if (course.difficultyLevel == "easy")
                                score += 10;
                            else if (course.difficultyLevel == "medium")
                                score += 20;
                            else if (course.difficultyLevel == "hard")
                                score += 25;
                        }

                        score += std::max(0, 10 - course.credits);
                        scores[i] = score;
                    }

                    std::vector<size_t> order(availableCourses.size());
                    for (size_t i = 0; i < order.size(); ++i)
                    {
                        order[i] = i;
                    }
                    std::stable_sort(
                        order.begin(),
                        order.end(),
                        [&scores](size_t a, size_t b) {
                            return scores[a] > scores[b];
                        });

                    Json::Value recommendations(Json::arrayValue);
                    const size_t limit =
                        maxRecommendations > 0
                            ? std::min<size_t>(
                                  static_cast<size_t>(maxRecommendations),
                                  order.size())
                            : 0;
                    for (size_t i = 0; i < limit; ++i)
                    {
                        const auto &course = availableCourses[order[i]];
                        Json::Value entry;
                        entry["id"] = Json::Int64(course.id);
                        entry["code"] = course.code;
                        entry["name"] = course.name;
                        entry["credits"] = course.credits;
                        entry["difficulty_level"] = course.difficultyLevel;
                        entry["reason"] = currentGpa.has_value()
                            ? "Eligible with prerequisites satisfied; ranked "
                              "by GPA, difficulty preference, and credits."
                            : "Eligible with prerequisites satisfied; ranked "
                              "without a GPA penalty because no grades exist.";
                        recommendations.append(std::move(entry));
                    }

                    Json::Value response;
                    response["student_id"] = Json::Int64(studentId);
                    response["recommendations"] = std::move(recommendations);
                    callback(ServiceResult::ok(std::move(response)));
                },
                [callback](const drogon::orm::DrogonDbException &exception) {
                    LOG_ERROR << "Failed to load course recommendations: "
                              << exception.base().what();
                    callback(ServiceResult::error(
                        "Unable to load course recommendations"));
                });
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to validate student: "
                      << exception.base().what();
            callback(
                ServiceResult::error("Unable to load course recommendations"));
        },
        studentId);
}

void StudentService::buildSemesterPlan(
    const drogon::orm::DbClientPtr &database,
    int64_t studentId,
    const std::optional<std::string> &preferredDifficulty,
    const std::optional<int64_t> &maxCredits,
    std::function<void(ServiceResult)> &&callback)
{
    database->execSqlAsync(
        "SELECT id, max_weekly_credits FROM students WHERE id = $1",
        [database,
         callback,
         studentId,
         preferredDifficulty,
         maxCredits](const drogon::orm::Result &students) {
            if (students.empty())
            {
                callback(ServiceResult::notFound("Student not found"));
                return;
            }

            const auto configuredMax =
                students.front()["max_weekly_credits"].as<int64_t>();
            if (maxCredits.has_value() && *maxCredits <= 0)
            {
                callback(ServiceResult::badRequest(
                    "max_credits must be a positive finite value"));
                return;
            }
            const int64_t effectiveMaxCredits =
                std::min(maxCredits.value_or(configuredMax), configuredMax);

            fetchAvailableCourses(
                database,
                studentId,
                [callback,
                 studentId,
                 preferredDifficulty,
                 effectiveMaxCredits](const drogon::orm::Result &courses) {
                    auto availableCourses = toAvailableCourses(courses);

                    std::vector<size_t> order(availableCourses.size());
                    for (size_t i = 0; i < order.size(); ++i)
                    {
                        order[i] = i;
                    }
                    std::stable_sort(
                        order.begin(),
                        order.end(),
                        [&availableCourses,
                         &preferredDifficulty](size_t a, size_t b) {
                            const bool matchesA =
                                preferredDifficulty.has_value() &&
                                availableCourses[a].difficultyLevel ==
                                    preferredDifficulty.value();
                            const bool matchesB =
                                preferredDifficulty.has_value() &&
                                availableCourses[b].difficultyLevel ==
                                    preferredDifficulty.value();
                            if (matchesA != matchesB)
                            {
                                return matchesA;
                            }
                            return availableCourses[a].credits <
                                   availableCourses[b].credits;
                        });

                    Json::Value coursesJson(Json::arrayValue);
                    int64_t totalCredits = 0;
                    int64_t estimatedWeeklyHours = 0;
                    for (const auto index : order)
                    {
                        const auto &course = availableCourses[index];
                        if (totalCredits + course.credits >
                            effectiveMaxCredits)
                        {
                            continue;
                        }

                        totalCredits += course.credits;
                        estimatedWeeklyHours += course.estimatedWeeklyHours;

                        Json::Value entry;
                        entry["id"] = Json::Int64(course.id);
                        entry["code"] = course.code;
                        entry["name"] = course.name;
                        entry["credits"] = course.credits;
                        entry["difficulty_level"] = course.difficultyLevel;
                        entry["estimated_weekly_hours"] =
                            course.estimatedWeeklyHours;
                        entry["reason"] =
                            "Selected because it is available and fits "
                            "within the credit limit.";
                        coursesJson.append(std::move(entry));
                    }

                    Json::Value response;
                    response["student_id"] = Json::Int64(studentId);
                    response["max_credits"] =
                        Json::Int64(effectiveMaxCredits);
                    response["total_credits"] = Json::Int64(totalCredits);
                    response["estimated_weekly_hours"] =
                        Json::Int64(estimatedWeeklyHours);
                    response["courses"] = std::move(coursesJson);
                    callback(ServiceResult::ok(std::move(response)));
                },
                [callback](const drogon::orm::DrogonDbException &exception) {
                    LOG_ERROR << "Failed to build semester plan: "
                              << exception.base().what();
                    callback(
                        ServiceResult::error("Unable to build semester plan"));
                });
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to validate student: "
                      << exception.base().what();
            callback(ServiceResult::error("Unable to build semester plan"));
        },
        studentId);
}

void StudentService::analyzeRisk(
    const drogon::orm::DbClientPtr &database,
    int64_t studentId,
    const std::vector<int64_t> &courseIds,
    std::function<void(ServiceResult)> &&callback)
{
    database->execSqlAsync(
        "SELECT id, current_gpa, max_weekly_credits FROM students "
        "WHERE id = $1",
        [database, callback, studentId, courseIds](
            const drogon::orm::Result &students) {
            if (students.empty())
            {
                callback(ServiceResult::notFound("Student not found"));
                return;
            }

            const std::optional<double> currentGpa =
                students.front()["current_gpa"].isNull()
                    ? std::nullopt
                    : std::optional<double>(
                          students.front()["current_gpa"].as<double>());
            const auto maxWeeklyCredits =
                students.front()["max_weekly_credits"].as<int>();

            std::vector<int64_t> uniqueCourseIds(courseIds.begin(),
                                                 courseIds.end());
            std::sort(uniqueCourseIds.begin(), uniqueCourseIds.end());
            uniqueCourseIds.erase(
                std::unique(uniqueCourseIds.begin(), uniqueCourseIds.end()),
                uniqueCourseIds.end());

            database->execSqlAsync(
                "SELECT id, credits, difficulty_level, "
                "estimated_weekly_hours FROM courses",
                [callback,
                 studentId,
                 currentGpa,
                 maxWeeklyCredits,
                 uniqueCourseIds](const drogon::orm::Result &courses) {
                    std::map<int64_t, AvailableCourse> coursesById;
                    for (const auto &row : courses)
                    {
                        AvailableCourse course;
                        course.id = row["id"].as<int64_t>();
                        course.credits = row["credits"].as<int>();
                        course.difficultyLevel =
                            row["difficulty_level"].as<std::string>();
                        course.estimatedWeeklyHours =
                            row["estimated_weekly_hours"].as<int>();
                        coursesById[course.id] = course;
                    }

                    for (const auto courseId : uniqueCourseIds)
                    {
                        if (coursesById.find(courseId) == coursesById.end())
                        {
                            callback(ServiceResult::badRequest(
                                "One or more course_ids were not found"));
                            return;
                        }
                    }

                    int64_t totalCredits = 0;
                    int64_t estimatedWeeklyHours = 0;
                    int64_t hardCoursesCount = 0;
                    for (const auto courseId : uniqueCourseIds)
                    {
                        const auto &course = coursesById.at(courseId);
                        totalCredits += course.credits;
                        estimatedWeeklyHours += course.estimatedWeeklyHours;
                        if (course.difficultyLevel == "hard")
                        {
                            ++hardCoursesCount;
                        }
                    }

                    int riskScore = 0;
                    Json::Value reasons(Json::arrayValue);
                    Json::Value recommendations(Json::arrayValue);

                    if (currentGpa.has_value() && *currentGpa < 60)
                    {
                        riskScore += 2;
                        reasons.append("The student's GPA is below 60.");
                        recommendations.append(
                            "Consider reducing the course load and focusing "
                            "on GPA recovery.");
                    }
                    else if (currentGpa.has_value() && *currentGpa < 70)
                    {
                        riskScore += 1;
                        reasons.append("The student's GPA is below 70.");
                        recommendations.append(
                            "Consider selecting easier courses or reducing "
                            "total credits.");
                    }

                    if (totalCredits > maxWeeklyCredits)
                    {
                        riskScore += 1;
                        reasons.append(
                            "The selected courses exceed the student's "
                            "maximum weekly credits.");
                        recommendations.append(
                            "Reduce the plan to fit within the student's "
                            "credit limit.");
                    }

                    if (estimatedWeeklyHours > 30)
                    {
                        riskScore += 1;
                        reasons.append(
                            "The selected courses require more than 30 "
                            "estimated weekly hours.");
                        recommendations.append(
                            "Consider replacing a high-workload course with "
                            "a lighter course.");
                    }

                    if (hardCoursesCount >= 3)
                    {
                        riskScore += 2;
                        reasons.append(
                            "The plan includes 3 or more hard courses.");
                        recommendations.append(
                            "Consider replacing at least one hard course "
                            "with a medium difficulty course.");
                    }
                    else if (hardCoursesCount >= 2)
                    {
                        riskScore += 1;
                        reasons.append("The plan includes 2 hard courses.");
                        recommendations.append(
                            "Consider replacing one hard course with a "
                            "medium difficulty course.");
                    }

                    std::string riskLevel;
                    if (riskScore >= 3)
                    {
                        riskLevel = "High";
                    }
                    else if (riskScore >= 1)
                    {
                        riskLevel = "Medium";
                    }
                    else
                    {
                        riskLevel = "Low";
                    }

                    if (riskLevel == "Low")
                    {
                        reasons.append(
                            "The selected course load appears manageable.");
                        recommendations.append(
                            "Maintain steady study habits and monitor "
                            "workload.");
                    }

                    Json::Value response;
                    response["student_id"] = Json::Int64(studentId);
                    response["risk_level"] = riskLevel;
                    response["total_credits"] = Json::Int64(totalCredits);
                    response["estimated_weekly_hours"] =
                        Json::Int64(estimatedWeeklyHours);
                    response["hard_courses_count"] =
                        Json::Int64(hardCoursesCount);
                    response["reasons"] = std::move(reasons);
                    response["recommendations"] = std::move(recommendations);
                    callback(ServiceResult::ok(std::move(response)));
                },
                [callback](const drogon::orm::DrogonDbException &exception) {
                    LOG_ERROR << "Failed to run risk analysis: "
                              << exception.base().what();
                    callback(
                        ServiceResult::error("Unable to run risk analysis"));
                });
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to validate student: "
                      << exception.base().what();
            callback(ServiceResult::error("Unable to run risk analysis"));
        },
        studentId);
}
