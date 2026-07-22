#include "ToolRegistry.h"

#include <drogon/drogon.h>

#include "CourseService.h"
#include "ServiceResult.h"
#include "StudentService.h"

namespace
{
Json::Value toToolResult(const ServiceResult &result)
{
    Json::Value toolResult;
    if (result.status == ServiceResult::Status::Ok)
    {
        toolResult["success"] = true;
        toolResult["data"] = result.data;
    }
    else
    {
        toolResult["success"] = false;
        toolResult["error"] = result.message;
    }
    return toolResult;
}

Json::Value toolFailure(const std::string &message)
{
    Json::Value toolResult;
    toolResult["success"] = false;
    toolResult["error"] = message;
    return toolResult;
}

bool tryGetStudentId(const Json::Value &args, int64_t &studentId, std::string &error)
{
    if (!args.isObject() || !args.isMember("student_id") ||
        !args["student_id"].isIntegral())
    {
        error = "student_id is required and must be an integer";
        return false;
    }
    studentId = args["student_id"].asInt64();
    return true;
}

bool tryGetCourseId(const Json::Value &args, int64_t &courseId, std::string &error)
{
    if (!args.isObject() || !args.isMember("course_id") ||
        !args["course_id"].isIntegral())
    {
        error = "course_id is required and must be an integer";
        return false;
    }
    courseId = args["course_id"].asInt64();
    return true;
}

bool tryGetOptionalDifficulty(const Json::Value &args,
                              bool &hasDifficulty,
                              std::string &difficulty,
                              std::string &error)
{
    hasDifficulty = false;
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
    if (!value.isString() || !StudentService::isValidDifficulty(value.asString()))
    {
        error = "difficulty must be one of: easy, medium, hard";
        return false;
    }
    difficulty = value.asString();
    hasDifficulty = true;
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

Json::Value ToolRegistry::toolDeclarations()
{
    Json::Value declarations(Json::arrayValue);

    {
        Json::Value properties;
        properties["student_id"] =
            property("integer", "The student's numeric id");
        Json::Value parameters;
        parameters["type"] = "object";
        parameters["properties"] = properties;
        Json::Value required(Json::arrayValue);
        required.append("student_id");
        parameters["required"] = required;
        declarations.append(declareTool(
            "get_student_profile",
            "Get a student's profile (name, email, department, year "
            "level, GPA, max weekly credits).",
            parameters));
    }

    {
        Json::Value properties;
        properties["student_id"] =
            property("integer", "The student's numeric id");
        Json::Value parameters;
        parameters["type"] = "object";
        parameters["properties"] = properties;
        Json::Value required(Json::arrayValue);
        required.append("student_id");
        parameters["required"] = required;
        declarations.append(declareTool(
            "get_academic_summary",
            "Get a student's academic summary: current GPA, and counts of "
            "completed/active/failed courses and completed credits.",
            parameters));
    }

    {
        Json::Value properties;
        properties["student_id"] =
            property("integer", "The student's numeric id");
        Json::Value parameters;
        parameters["type"] = "object";
        parameters["properties"] = properties;
        Json::Value required(Json::arrayValue);
        required.append("student_id");
        parameters["required"] = required;
        declarations.append(declareTool(
            "get_available_courses",
            "Get the list of courses a student is eligible to take: not "
            "already active/completed, and all prerequisites satisfied.",
            parameters));
    }

    {
        Json::Value properties;
        properties["student_id"] =
            property("integer", "The student's numeric id");
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
        declarations.append(declareTool(
            "get_course_recommendations",
            "Get scored course recommendations for a student from their "
            "available courses, based on GPA and optional preferred "
            "difficulty.",
            parameters));
    }

    {
        Json::Value properties;
        properties["student_id"] =
            property("integer", "The student's numeric id");
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
        declarations.append(declareTool(
            "build_semester_plan",
            "Greedily build a semester plan from a student's available "
            "courses that fits within a credit limit.",
            parameters));
    }

    {
        Json::Value properties;
        properties["student_id"] =
            property("integer", "The student's numeric id");
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
        declarations.append(declareTool(
            "analyze_academic_risk",
            "Analyze the academic risk (Low/Medium/High) of a student "
            "taking a specific set of courses together, based on GPA, "
            "credit load, workload, and course difficulty.",
            parameters));
    }

    {
        Json::Value properties;
        properties["course_id"] =
            property("integer", "The course's numeric id");
        Json::Value parameters;
        parameters["type"] = "object";
        parameters["properties"] = properties;
        Json::Value required(Json::arrayValue);
        required.append("course_id");
        parameters["required"] = required;
        declarations.append(declareTool(
            "get_course_details",
            "Get full details for a course: description, credits, "
            "difficulty, weekly hours, instructor, and prerequisites.",
            parameters));
    }

    {
        Json::Value properties;
        properties["department"] =
            property("string", "Filter by department name");
        properties["difficulty"] =
            difficultyProperty("Filter by difficulty level");
        properties["credits"] = property("integer", "Filter by exact credit count");
        properties["instructor"] =
            property("string", "Filter by instructor name (partial match)");
        Json::Value parameters;
        parameters["type"] = "object";
        parameters["properties"] = properties;
        declarations.append(declareTool(
            "search_courses",
            "Search the full course catalog with optional filters on "
            "department, difficulty, credits, and instructor name.",
            parameters));
    }

    return declarations;
}

void ToolRegistry::execute(
    const drogon::orm::DbClientPtr &database,
    const std::string &toolName,
    const Json::Value &args,
    std::function<void(Json::Value)> &&callback)
{
    std::string error;

    if (toolName == "get_student_profile")
    {
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
        return;
    }

    if (toolName == "get_academic_summary")
    {
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
        return;
    }

    if (toolName == "get_available_courses")
    {
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
        return;
    }

    if (toolName == "get_course_recommendations")
    {
        int64_t studentId = 0;
        if (!tryGetStudentId(args, studentId, error))
        {
            callback(toolFailure(error));
            return;
        }
        bool hasDifficulty = false;
        std::string difficulty;
        if (!tryGetOptionalDifficulty(args, hasDifficulty, difficulty, error))
        {
            callback(toolFailure(error));
            return;
        }
        int64_t maxRecommendations = 3;
        if (args.isObject() && args.isMember("max_recommendations") &&
            !args["max_recommendations"].isNull())
        {
            if (!args["max_recommendations"].isIntegral())
            {
                callback(toolFailure("max_recommendations must be an integer"));
                return;
            }
            maxRecommendations = args["max_recommendations"].asInt64();
        }
        StudentService::getCourseRecommendations(
            database,
            studentId,
            hasDifficulty,
            difficulty,
            maxRecommendations,
            [callback](ServiceResult result) { callback(toToolResult(result)); });
        return;
    }

    if (toolName == "build_semester_plan")
    {
        int64_t studentId = 0;
        if (!tryGetStudentId(args, studentId, error))
        {
            callback(toolFailure(error));
            return;
        }
        bool hasDifficulty = false;
        std::string difficulty;
        if (!tryGetOptionalDifficulty(args, hasDifficulty, difficulty, error))
        {
            callback(toolFailure(error));
            return;
        }
        bool hasMaxCredits = false;
        int64_t maxCredits = 0;
        if (args.isObject() && args.isMember("max_credits") &&
            !args["max_credits"].isNull())
        {
            if (!args["max_credits"].isIntegral())
            {
                callback(toolFailure("max_credits must be an integer"));
                return;
            }
            maxCredits = args["max_credits"].asInt64();
            hasMaxCredits = true;
        }
        StudentService::buildSemesterPlan(
            database,
            studentId,
            hasDifficulty,
            difficulty,
            hasMaxCredits,
            maxCredits,
            [callback](ServiceResult result) { callback(toToolResult(result)); });
        return;
    }

    if (toolName == "analyze_academic_risk")
    {
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
            if (!element.isIntegral())
            {
                callback(toolFailure("course_ids must contain only integers"));
                return;
            }
            courseIds.push_back(element.asInt64());
        }
        StudentService::analyzeRisk(
            database,
            studentId,
            courseIds,
            [callback](ServiceResult result) { callback(toToolResult(result)); });
        return;
    }

    if (toolName == "get_course_details")
    {
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
        return;
    }

    if (toolName == "search_courses")
    {
        CourseSearchFilters filters;
        if (args.isObject() && args.isMember("department") &&
            args["department"].isString() && !args["department"].asString().empty())
        {
            filters.hasDepartment = true;
            filters.department = args["department"].asString();
        }
        bool hasDifficulty = false;
        std::string difficulty;
        if (!tryGetOptionalDifficulty(args, hasDifficulty, difficulty, error))
        {
            callback(toolFailure(error));
            return;
        }
        filters.hasDifficulty = hasDifficulty;
        filters.difficulty = difficulty;
        if (args.isObject() && args.isMember("credits") &&
            !args["credits"].isNull())
        {
            if (!args["credits"].isIntegral())
            {
                callback(toolFailure("credits must be an integer"));
                return;
            }
            filters.hasCredits = true;
            filters.credits = args["credits"].asInt();
        }
        if (args.isObject() && args.isMember("instructor") &&
            args["instructor"].isString() && !args["instructor"].asString().empty())
        {
            filters.hasInstructor = true;
            filters.instructor = args["instructor"].asString();
        }
        CourseService::searchCourses(
            database, filters, [callback](ServiceResult result) {
                callback(toToolResult(result));
            });
        return;
    }

    callback(toolFailure("Unknown tool: " + toolName));
}
