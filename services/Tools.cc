#include "Tools.h"

#include <limits>
#include <optional>

#include <drogon/drogon.h>

#include "CourseService.h"
#include "EnrollmentService.h"
#include "StudentService.h"
#include "ToolResult.h"
#include "ValidationHelpers.h"

namespace
{
bool tryGetStudentId(const Json::Value &args, int64_t &studentId, std::string &error)
{
    if (!args.isObject() || !args.isMember("student_id") ||
        !ValidationHelpers::tryGetInt64(args["student_id"], studentId, error))
    {
        error = "student_id is required and must be an integer";
        return false;
    }
    return true;
}

bool tryGetCourseId(const Json::Value &args, int64_t &courseId, std::string &error)
{
    if (!args.isObject() || !args.isMember("course_id") ||
        !ValidationHelpers::tryGetInt64(args["course_id"], courseId, error))
    {
        error = "course_id is required and must be an integer";
        return false;
    }
    return true;
}

bool tryGetOptionalDifficulty(const Json::Value &args,
                              std::optional<std::string> &difficulty,
                              std::string &error)
{
    difficulty.reset();
    if (!args.isObject() ||
        !(args.isMember("preferred_difficulty") ||
          args.isMember("difficulty")))
    {
        return true;
    }

    const auto &value = args.isMember("preferred_difficulty")
                             ? args["preferred_difficulty"]
                             : args["difficulty"];
    if (value.isNull())
    {
        return true;
    }
    if (!value.isString() ||
        !ValidationHelpers::validateDifficultyString(
            value.asString(), "difficulty", error))
    {
        error = "difficulty must be one of: easy, medium, hard";
        return false;
    }
    difficulty = value.asString();
    return true;
}

Json::Value declareTool(const std::string &name,
                        const std::string &description,
                        Json::Value parameters)
{
    Json::Value tool;
    tool["name"] = name;
    tool["description"] = description;
    tool["parameters"] = std::move(parameters);
    return tool;
}

Json::Value property(const std::string &type, const std::string &description)
{
    Json::Value property;
    property["type"] = type;
    property["description"] = description;
    return property;
}

Json::Value difficultyProperty(const std::string &description)
{
    Json::Value property;
    property["type"] = "string";
    property["description"] = description;
    Json::Value values(Json::arrayValue);
    values.append("easy");
    values.append("medium");
    values.append("hard");
    property["enum"] = std::move(values);
    return property;
}
}  // namespace

Json::Value GetStudentProfileTool::declaration() const
{
    Json::Value properties;
    properties["student_id"] = property("integer", "The student's numeric id");
    Json::Value parameters;
    parameters["type"] = "object";
    parameters["properties"] = properties;
    Json::Value required(Json::arrayValue);
    required.append("student_id");
    parameters["required"] = required;
    return declareTool(
        "get_student_profile",
        "Get a student's profile (name, email, department, year "
        "level, GPA, max weekly credits).",
        parameters);
}

void GetStudentProfileTool::execute(
    const drogon::orm::DbClientPtr &database,
    const Json::Value &args,
    std::function<void(Json::Value)> &&callback) const
{
    std::string error;
    int64_t studentId = 0;
    if (!tryGetStudentId(args, studentId, error))
    {
        callback(toolFailure(error));
        return;
    }
    StudentService::getProfile(
        database, studentId, [callback](ServiceResult result) {
            callback(toToolResult(result));
        });
}

Json::Value GetAcademicSummaryTool::declaration() const
{
    Json::Value properties;
    properties["student_id"] = property("integer", "The student's numeric id");
    Json::Value parameters;
    parameters["type"] = "object";
    parameters["properties"] = properties;
    Json::Value required(Json::arrayValue);
    required.append("student_id");
    parameters["required"] = required;
    return declareTool(
        "get_academic_summary",
        "Get a student's academic summary: current GPA, and counts of "
        "completed/active/failed courses and completed credits.",
        parameters);
}

void GetAcademicSummaryTool::execute(
    const drogon::orm::DbClientPtr &database,
    const Json::Value &args,
    std::function<void(Json::Value)> &&callback) const
{
    std::string error;
    int64_t studentId = 0;
    if (!tryGetStudentId(args, studentId, error))
    {
        callback(toolFailure(error));
        return;
    }
    StudentService::getAcademicSummary(
        database, studentId, [callback](ServiceResult result) {
            callback(toToolResult(result));
        });
}

Json::Value GetAvailableCoursesTool::declaration() const
{
    Json::Value properties;
    properties["student_id"] = property("integer", "The student's numeric id");
    Json::Value parameters;
    parameters["type"] = "object";
    parameters["properties"] = properties;
    Json::Value required(Json::arrayValue);
    required.append("student_id");
    parameters["required"] = required;
    return declareTool(
        "get_available_courses",
        "Get the list of courses a student is eligible to take: not "
        "already active/completed, and all prerequisites satisfied.",
        parameters);
}

void GetAvailableCoursesTool::execute(
    const drogon::orm::DbClientPtr &database,
    const Json::Value &args,
    std::function<void(Json::Value)> &&callback) const
{
    std::string error;
    int64_t studentId = 0;
    if (!tryGetStudentId(args, studentId, error))
    {
        callback(toolFailure(error));
        return;
    }
    StudentService::getAvailableCourses(
        database, studentId, [callback](ServiceResult result) {
            callback(toToolResult(result));
        });
}

Json::Value GetCourseRecommendationsTool::declaration() const
{
    Json::Value properties;
    properties["student_id"] = property("integer", "The student's numeric id");
    properties["preferred_difficulty"] = difficultyProperty(
        "Optional preferred course difficulty level");
    properties["max_recommendations"] = property(
        "integer", "Maximum number of recommendations to return (default 3)");
    Json::Value parameters;
    parameters["type"] = "object";
    parameters["properties"] = properties;
    Json::Value required(Json::arrayValue);
    required.append("student_id");
    parameters["required"] = required;
    return declareTool(
        "get_course_recommendations",
        "Get scored course recommendations for a student from their "
        "available courses, based on GPA and optional preferred "
        "difficulty.",
        parameters);
}

void GetCourseRecommendationsTool::execute(
    const drogon::orm::DbClientPtr &database,
    const Json::Value &args,
    std::function<void(Json::Value)> &&callback) const
{
    std::string error;
    int64_t studentId = 0;
    if (!tryGetStudentId(args, studentId, error))
    {
        callback(toolFailure(error));
        return;
    }
    std::optional<std::string> difficulty;
    if (!tryGetOptionalDifficulty(args, difficulty, error))
    {
        callback(toolFailure(error));
        return;
    }
    int64_t maxRecommendations = 3;
    if (args.isObject() && args.isMember("max_recommendations") &&
        !args["max_recommendations"].isNull())
    {
        if (!ValidationHelpers::tryGetInt64(
                args["max_recommendations"], maxRecommendations, error))
        {
            callback(toolFailure("max_recommendations must be an integer"));
            return;
        }
    }
    StudentService::getCourseRecommendations(
        database,
        studentId,
        difficulty,
        maxRecommendations,
        [callback](ServiceResult result) { callback(toToolResult(result)); });
}

Json::Value BuildSemesterPlanTool::declaration() const
{
    Json::Value properties;
    properties["student_id"] = property("integer", "The student's numeric id");
    properties["max_credits"] = property(
        "integer",
        "Maximum total credits for the semester (default: the "
        "student's max_weekly_credits)");
    properties["preferred_difficulty"] = difficultyProperty(
        "Optional preferred course difficulty level");
    Json::Value parameters;
    parameters["type"] = "object";
    parameters["properties"] = properties;
    Json::Value required(Json::arrayValue);
    required.append("student_id");
    parameters["required"] = required;
    return declareTool(
        "build_semester_plan",
        "Greedily build a semester plan from a student's available "
        "courses that fits within a credit limit.",
        parameters);
}

void BuildSemesterPlanTool::execute(
    const drogon::orm::DbClientPtr &database,
    const Json::Value &args,
    std::function<void(Json::Value)> &&callback) const
{
    std::string error;
    int64_t studentId = 0;
    if (!tryGetStudentId(args, studentId, error))
    {
        callback(toolFailure(error));
        return;
    }
    std::optional<std::string> difficulty;
    if (!tryGetOptionalDifficulty(args, difficulty, error))
    {
        callback(toolFailure(error));
        return;
    }
    std::optional<int64_t> maxCredits;
    if (args.isObject() && args.isMember("max_credits") &&
        !args["max_credits"].isNull())
    {
        int64_t value = 0;
        if (!ValidationHelpers::tryGetInt64(args["max_credits"], value, error))
        {
            callback(toolFailure("max_credits must be an integer"));
            return;
        }
        maxCredits = value;
    }
    StudentService::buildSemesterPlan(
        database,
        studentId,
        difficulty,
        maxCredits,
        [callback](ServiceResult result) { callback(toToolResult(result)); });
}

Json::Value AnalyzeAcademicRiskTool::declaration() const
{
    Json::Value properties;
    properties["student_id"] = property("integer", "The student's numeric id");
    Json::Value courseIds;
    courseIds["type"] = "array";
    courseIds["description"] = "The course ids to analyze together";
    courseIds["items"] = property("integer", "A course id");
    properties["course_ids"] = courseIds;
    Json::Value parameters;
    parameters["type"] = "object";
    parameters["properties"] = properties;
    Json::Value required(Json::arrayValue);
    required.append("student_id");
    required.append("course_ids");
    parameters["required"] = required;
    return declareTool(
        "analyze_academic_risk",
        "Analyze the academic risk (Low/Medium/High) of a student "
        "taking a specific set of courses together, based on GPA, "
        "credit load, workload, and course difficulty.",
        parameters);
}

void AnalyzeAcademicRiskTool::execute(
    const drogon::orm::DbClientPtr &database,
    const Json::Value &args,
    std::function<void(Json::Value)> &&callback) const
{
    std::string error;
    int64_t studentId = 0;
    if (!tryGetStudentId(args, studentId, error))
    {
        callback(toolFailure(error));
        return;
    }
    if (!args.isObject() || !args.isMember("course_ids") ||
        !args["course_ids"].isArray() || args["course_ids"].empty())
    {
        callback(toolFailure("course_ids must be a non-empty list of integers"));
        return;
    }
    std::vector<int64_t> courseIds;
    courseIds.reserve(args["course_ids"].size());
    for (const auto &element : args["course_ids"])
    {
        int64_t courseIdValue = 0;
        if (!ValidationHelpers::tryGetInt64(element, courseIdValue, error))
        {
            callback(toolFailure("course_ids must contain only integers"));
            return;
        }
        courseIds.push_back(courseIdValue);
    }
    StudentService::analyzeRisk(
        database,
        studentId,
        courseIds,
        [callback](ServiceResult result) { callback(toToolResult(result)); });
}

Json::Value GetCourseDetailsTool::declaration() const
{
    Json::Value properties;
    properties["course_id"] = property("integer", "The course's numeric id");
    Json::Value parameters;
    parameters["type"] = "object";
    parameters["properties"] = properties;
    Json::Value required(Json::arrayValue);
    required.append("course_id");
    parameters["required"] = required;
    return declareTool(
        "get_course_details",
        "Get full details for a course: description, credits, "
        "difficulty, weekly hours, instructor, and prerequisites.",
        parameters);
}

void GetCourseDetailsTool::execute(
    const drogon::orm::DbClientPtr &database,
    const Json::Value &args,
    std::function<void(Json::Value)> &&callback) const
{
    std::string error;
    int64_t courseId = 0;
    if (!tryGetCourseId(args, courseId, error))
    {
        callback(toolFailure(error));
        return;
    }
    CourseService::getCourseDetails(
        database, courseId, [callback](ServiceResult result) {
            callback(toToolResult(result));
        });
}

Json::Value SearchCoursesTool::declaration() const
{
    Json::Value properties;
    properties["department"] = property("string", "Filter by department name");
    properties["difficulty"] = difficultyProperty("Filter by difficulty level");
    properties["credits"] = property("integer", "Filter by exact credit count");
    properties["instructor"] =
        property("string", "Filter by instructor name (partial match)");
    Json::Value parameters;
    parameters["type"] = "object";
    parameters["properties"] = properties;
    return declareTool(
        "search_courses",
        "Search the full course catalog with optional filters on "
        "department, difficulty, credits, and instructor name.",
        parameters);
}

void SearchCoursesTool::execute(
    const drogon::orm::DbClientPtr &database,
    const Json::Value &args,
    std::function<void(Json::Value)> &&callback) const
{
    std::string error;
    CourseSearchFilters filters;
    if (args.isObject() && args.isMember("department") &&
        args["department"].isString() && !args["department"].asString().empty())
    {
        filters.department = args["department"].asString();
    }
    if (!tryGetOptionalDifficulty(args, filters.difficulty, error))
    {
        callback(toolFailure(error));
        return;
    }
    if (args.isObject() && args.isMember("credits") &&
        !args["credits"].isNull())
    {
        int64_t creditsValue = 0;
        if (!ValidationHelpers::tryGetInt64(
                args["credits"], creditsValue, error) ||
            creditsValue < std::numeric_limits<int>::min() ||
            creditsValue > std::numeric_limits<int>::max())
        {
            callback(toolFailure("credits must be an integer"));
            return;
        }
        filters.credits = static_cast<int>(creditsValue);
    }
    if (args.isObject() && args.isMember("instructor") &&
        args["instructor"].isString() && !args["instructor"].asString().empty())
    {
        filters.instructor = args["instructor"].asString();
    }
    CourseService::searchCourses(
        database, filters, [callback](ServiceResult result) {
            callback(toToolResult(result));
        });
}

Json::Value EnrollInCourseTool::declaration() const
{
    Json::Value properties;
    properties["student_id"] = property("integer", "The student's numeric id");
    properties["course_id"] = property("integer", "The course's numeric id");
    properties["semester"] =
        property("string", "The semester to enroll in, e.g. '2026-Fall'");
    Json::Value parameters;
    parameters["type"] = "object";
    parameters["properties"] = properties;
    Json::Value required(Json::arrayValue);
    required.append("student_id");
    required.append("course_id");
    required.append("semester");
    parameters["required"] = required;
    return declareTool(
        "enroll_in_course",
        "Enroll a student in a course for a given semester, with status "
        "'planned'. Fails if the student or course doesn't exist, or if "
        "an enrollment already exists for that student/course/semester.",
        parameters);
}

void EnrollInCourseTool::execute(
    const drogon::orm::DbClientPtr &database,
    const Json::Value &args,
    std::function<void(Json::Value)> &&callback) const
{
    std::string error;
    int64_t studentId = 0;
    if (!tryGetStudentId(args, studentId, error))
    {
        callback(toolFailure(error));
        return;
    }
    int64_t courseId = 0;
    if (!tryGetCourseId(args, courseId, error))
    {
        callback(toolFailure(error));
        return;
    }
    if (!args.isObject() || !args.isMember("semester") ||
        !args["semester"].isString() || args["semester"].asString().empty())
    {
        callback(toolFailure("semester is required and must be a non-empty string"));
        return;
    }
    EnrollmentService::create(
        database,
        studentId,
        courseId,
        args["semester"].asString(),
        [callback](ServiceResult result) { callback(toToolResult(result)); });
}
