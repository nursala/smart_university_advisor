# Advanced C++ Batch 3 (Polymorphism, Templates, Concurrency) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Demonstrate genuine inheritance/polymorphism (`ToolRegistry`'s if/else chain becomes a real `Tool` base class + 8 overrides), a genuine template (`toJsonArray`), `std::optional<T>` instead of has-X/X boolean pairs, and real (not hypothetical) multi-threaded concurrency, with load-test evidence in `docs/concurrency-test.md`.

**Architecture:** `Tool` (abstract base, pure virtual `declaration()`/`execute()`) gets one concrete subclass per existing tool, moved verbatim from `ToolRegistry.cc`'s if-branches into `services/Tools.cc`. `ToolRegistry` keeps its existing static public interface (zero changes for `AgentController`/tests) but internally becomes a `std::map<std::string, const Tool*>` built once from a `std::vector<std::unique_ptr<Tool>>`. Every has-X/X boolean pair becomes `std::optional<T>`. A `toJsonArray` template replaces 2 of the repeated manual Result→Json::Value loops. `number_of_threads` goes from 1 to 4, the DB pool size becomes env-configurable (`DB_POOL_SIZE`, default 4), and both the enrollment-uniqueness race and `/agent/query` per-request state isolation are load-tested against the real, rebuilt, multi-threaded Docker stack — the latter via a small local mock Gemini server (`test/mock_gemini_server.js`) since no `GEMINI_API_KEY` is configured in this environment, reachable from the container via `GEMINI_API_HOST=http://host.docker.internal:<port>` (a new, defaulted-to-real-host env var added to `GeminiClient` purely for this).

**Tech Stack:** C++17, Drogon, jsoncpp, PostgreSQL, CMake, Docker Compose, Node.js (test-only mock server).

## Global Constraints

- No behavior change for any currently-passing request: same status codes, same response JSON shape, same error message text, for every case that worked before.
- `ToolRegistry`'s public static interface (`toolDeclarations()`, `execute(...)`) must not change signature — `AgentController.cc` and `test/test_main.cc` must need zero edits.
- New source files under `services/` are picked up automatically by `aux_source_directory(services SERVICE_SRC)` in `CMakeLists.txt` — no build file changes needed.
- Verify by actually building and running the stack (`docker compose up --build -d`) and hitting endpoints with `curl`/parallel load, not just by reading the code.
- Do not manufacture a `std::variant` or multiple-inheritance use case — explicitly out of scope per the task spec.
- No real rubric document exists in this repo (confirmed: only `README.md` and the prior `docs/superpowers/plans/2026-07-22-audit-fixes-batch1.md`) — this plan is the working spec.
- `.env`/`.env.example` ship `GEMINI_API_KEY` empty — no real Gemini key is available in this environment; the `/agent/query` concurrency test must use the local mock server, and this substitution must be reported explicitly (same posture as Batch 1's Gemini-path substitution).

---

### Task 1: `Tool` abstract base + 8 concrete tool classes + thin `ToolRegistry`

**Files:**
- Create: `services/Tool.h`
- Create: `services/ToolResult.h`
- Create: `services/Tools.h`
- Create: `services/Tools.cc`
- Modify: `services/ToolRegistry.cc` (full rewrite)
- `services/ToolRegistry.h`: unchanged (public interface is identical)

**Interfaces produced (used by Task 2):**
- `class Tool` with pure virtual `Json::Value declaration() const` and `void execute(const drogon::orm::DbClientPtr&, const Json::Value&, std::function<void(Json::Value)>&&) const`
- `toToolResult(const ServiceResult&)`, `toolFailure(const std::string&)` (moved from `ToolRegistry.cc`'s anonymous namespace, now shared via `ToolResult.h`)
- 8 concrete classes in `Tools.h`: `GetStudentProfileTool`, `GetAcademicSummaryTool`, `GetAvailableCoursesTool`, `GetCourseRecommendationsTool`, `BuildSemesterPlanTool`, `AnalyzeAcademicRiskTool`, `GetCourseDetailsTool`, `SearchCoursesTool`

- [ ] **Step 1: Create `services/Tool.h`**

```cpp
#pragma once

#include <drogon/orm/DbClient.h>
#include <json/json.h>

#include <functional>

// Abstract base for one agent function-tool. Each concrete Tool wraps the
// same service-layer call the corresponding REST endpoint uses; no
// business logic lives here or in ToolRegistry.
class Tool
{
  public:
    virtual ~Tool() = default;

    // This tool's Gemini `functionDeclarations` entry (name/description/
    // parameters). ToolRegistry uses declaration()["name"] as the lookup
    // key, so it must be unique and stable.
    virtual Json::Value declaration() const = 0;

    // Executes the tool with `args` (the JSON object Gemini sent as
    // functionCall.args). Always invokes `callback` with a JSON object
    // shaped either {"success": true, "data": ...} or
    // {"success": false, "error": "..."}.
    virtual void execute(
        const drogon::orm::DbClientPtr &database,
        const Json::Value &args,
        std::function<void(Json::Value)> &&callback) const = 0;
};
```

- [ ] **Step 2: Create `services/ToolResult.h`**

```cpp
#pragma once

#include <json/json.h>

#include <string>

#include "ServiceResult.h"

inline Json::Value toToolResult(const ServiceResult &result)
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

inline Json::Value toolFailure(const std::string &message)
{
    Json::Value toolResult;
    toolResult["success"] = false;
    toolResult["error"] = message;
    return toolResult;
}
```

- [ ] **Step 3: Create `services/Tools.h`**

```cpp
#pragma once

#include "Tool.h"

class GetStudentProfileTool : public Tool
{
  public:
    Json::Value declaration() const override;
    void execute(const drogon::orm::DbClientPtr &database,
                 const Json::Value &args,
                 std::function<void(Json::Value)> &&callback) const override;
};

class GetAcademicSummaryTool : public Tool
{
  public:
    Json::Value declaration() const override;
    void execute(const drogon::orm::DbClientPtr &database,
                 const Json::Value &args,
                 std::function<void(Json::Value)> &&callback) const override;
};

class GetAvailableCoursesTool : public Tool
{
  public:
    Json::Value declaration() const override;
    void execute(const drogon::orm::DbClientPtr &database,
                 const Json::Value &args,
                 std::function<void(Json::Value)> &&callback) const override;
};

class GetCourseRecommendationsTool : public Tool
{
  public:
    Json::Value declaration() const override;
    void execute(const drogon::orm::DbClientPtr &database,
                 const Json::Value &args,
                 std::function<void(Json::Value)> &&callback) const override;
};

class BuildSemesterPlanTool : public Tool
{
  public:
    Json::Value declaration() const override;
    void execute(const drogon::orm::DbClientPtr &database,
                 const Json::Value &args,
                 std::function<void(Json::Value)> &&callback) const override;
};

class AnalyzeAcademicRiskTool : public Tool
{
  public:
    Json::Value declaration() const override;
    void execute(const drogon::orm::DbClientPtr &database,
                 const Json::Value &args,
                 std::function<void(Json::Value)> &&callback) const override;
};

class GetCourseDetailsTool : public Tool
{
  public:
    Json::Value declaration() const override;
    void execute(const drogon::orm::DbClientPtr &database,
                 const Json::Value &args,
                 std::function<void(Json::Value)> &&callback) const override;
};

class SearchCoursesTool : public Tool
{
  public:
    Json::Value declaration() const override;
    void execute(const drogon::orm::DbClientPtr &database,
                 const Json::Value &args,
                 std::function<void(Json::Value)> &&callback) const override;
};
```

- [ ] **Step 4: Create `services/Tools.cc`**, moving every helper and if-branch body from the current `services/ToolRegistry.cc` verbatim (no logic changes — this is a pure move+split):

```cpp
#include "Tools.h"

#include <limits>

#include <drogon/drogon.h>

#include "CourseService.h"
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
    if (!value.isString() ||
        !ValidationHelpers::validateDifficultyString(
            value.asString(), "difficulty", error))
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
        hasDifficulty,
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
        if (!ValidationHelpers::tryGetInt64(
                args["max_credits"], maxCredits, error))
        {
            callback(toolFailure("max_credits must be an integer"));
            return;
        }
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
        int64_t creditsValue = 0;
        if (!ValidationHelpers::tryGetInt64(
                args["credits"], creditsValue, error) ||
            creditsValue < std::numeric_limits<int>::min() ||
            creditsValue > std::numeric_limits<int>::max())
        {
            callback(toolFailure("credits must be an integer"));
            return;
        }
        filters.hasCredits = true;
        filters.credits = static_cast<int>(creditsValue);
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
}
```

- [ ] **Step 5: Rewrite `services/ToolRegistry.cc`**

```cpp
#include "ToolRegistry.h"

#include <map>
#include <memory>
#include <vector>

#include "Tool.h"
#include "ToolResult.h"
#include "Tools.h"

namespace
{
std::vector<std::unique_ptr<Tool>> buildTools()
{
    std::vector<std::unique_ptr<Tool>> tools;
    tools.push_back(std::make_unique<GetStudentProfileTool>());
    tools.push_back(std::make_unique<GetAcademicSummaryTool>());
    tools.push_back(std::make_unique<GetAvailableCoursesTool>());
    tools.push_back(std::make_unique<GetCourseRecommendationsTool>());
    tools.push_back(std::make_unique<BuildSemesterPlanTool>());
    tools.push_back(std::make_unique<AnalyzeAcademicRiskTool>());
    tools.push_back(std::make_unique<GetCourseDetailsTool>());
    tools.push_back(std::make_unique<SearchCoursesTool>());
    return tools;
}

const std::vector<std::unique_ptr<Tool>> &tools()
{
    static const std::vector<std::unique_ptr<Tool>> instance = buildTools();
    return instance;
}

const std::map<std::string, const Tool *> &toolsByName()
{
    static const std::map<std::string, const Tool *> instance = [] {
        std::map<std::string, const Tool *> byName;
        for (const auto &tool : tools())
        {
            byName[tool->declaration()["name"].asString()] = tool.get();
        }
        return byName;
    }();
    return instance;
}
}  // namespace

Json::Value ToolRegistry::toolDeclarations()
{
    Json::Value declarations(Json::arrayValue);
    for (const auto &tool : tools())
    {
        declarations.append(tool->declaration());
    }
    return declarations;
}

void ToolRegistry::execute(
    const drogon::orm::DbClientPtr &database,
    const std::string &toolName,
    const Json::Value &args,
    std::function<void(Json::Value)> &&callback)
{
    const auto &byName = toolsByName();
    const auto it = byName.find(toolName);
    if (it == byName.end())
    {
        callback(toolFailure("Unknown tool: " + toolName));
        return;
    }

    // Defense-in-depth backstop: every known validation gap is closed
    // inside each Tool's execute(), but a future one degrading to a clean
    // tool-level error (instead of an uncaught exception surfacing in the
    // async Gemini-response callback in AgentController) is much safer
    // than crashing the process. `callback` is copied into the virtual
    // call so it is still available here if that call throws before
    // invoking it.
    try
    {
        it->second->execute(
            database, args, std::function<void(Json::Value)>(callback));
    }
    catch (const std::exception &exception)
    {
        callback(toolFailure(std::string("Unexpected tool error: ") +
                             exception.what()));
    }
}
```

- [ ] **Step 6: Build and run the existing DROGON_TEST case + full stack, confirm identical behavior**

**Verification:**
```sh
docker compose up --build -d
docker compose exec api sh -c "cd /build/test && ./smart_university_advisor_test" 2>&1 | tail -20
# Expect: both DROGON_TEST cases still pass (BasicTest, ToolRegistryRejectsInt64OverflowWithoutThrowing)

curl -s -w "\n%{http_code}\n" http://localhost:8080/students/1/profile
curl -s -w "\n%{http_code}\n" -X POST http://localhost:8080/students/1/course-recommendations -H "Content-Type: application/json" -d '{"max_recommendations": 2}'
curl -s -w "\n%{http_code}\n" http://localhost:8080/courses/1/details
curl -s -w "\n%{http_code}\n" "http://localhost:8080/courses?difficulty=hard"
# Expect: identical status/body shape to pre-refactor behavior for all 8 tool-backing endpoints

grep -rn "virtual\|override" services/
# Expect: hits in Tool.h (1 virtual dtor + 2 pure virtual) and Tools.h (16 overrides across 8 classes) — real new code, not Drogon headers
```

---

### Task 2a: `std::optional<T>` for `CourseSearchFilters` (CourseService + CoursesController)

**Files:**
- Modify: `services/CourseService.h` (`CourseSearchFilters` struct)
- Modify: `services/CourseService.cc` (`searchCourses` filtering)
- Modify: `controllers/CoursesController.cc` (`list`)
- Modify: `services/Tools.cc` (`SearchCoursesTool::execute`, built in Task 1)

**Interfaces produced:**
- `CourseSearchFilters { std::optional<std::string> department; std::optional<std::string> difficulty; std::optional<int> credits; std::optional<std::string> instructor; }`

- [ ] **Step 1: `services/CourseService.h`** — replace the 4 has-X/X pairs:

```cpp
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
```

- [ ] **Step 2: `services/CourseService.cc`** — in `searchCourses`'s per-row filter block, replace:

```cpp
                if (filters.hasDepartment &&
                    toLower(department) != toLower(filters.department))
                {
                    continue;
                }
                if (filters.hasDifficulty &&
                    difficultyLevel != filters.difficulty)
                {
                    continue;
                }
                if (filters.hasCredits && credits != filters.credits)
                {
                    continue;
                }
                if (filters.hasInstructor &&
                    !containsCaseInsensitive(instructorName,
                                             filters.instructor))
                {
                    continue;
                }
```

with:

```cpp
                if (filters.department.has_value() &&
                    toLower(department) != toLower(filters.department.value()))
                {
                    continue;
                }
                if (filters.difficulty.has_value() &&
                    difficultyLevel != filters.difficulty.value())
                {
                    continue;
                }
                if (filters.credits.has_value() &&
                    credits != filters.credits.value())
                {
                    continue;
                }
                if (filters.instructor.has_value() &&
                    !containsCaseInsensitive(instructorName,
                                             filters.instructor.value()))
                {
                    continue;
                }
```

- [ ] **Step 3: `controllers/CoursesController.cc`** — in `list`, drop each `hasX = true` line and assign the optional directly:

```cpp
    const auto department = request->getParameter("department");
    if (!department.empty())
    {
        filters.department = department;
    }

    const auto difficulty = request->getParameter("difficulty");
    if (!difficulty.empty())
    {
        std::string difficultyError;
        if (!ValidationHelpers::validateDifficultyString(
                difficulty, "difficulty", difficultyError))
        {
            callback(errorResponse(difficultyError, drogon::k400BadRequest));
            return;
        }
        filters.difficulty = difficulty;
    }

    const auto creditsParam = request->getParameter("credits");
    if (!creditsParam.empty())
    {
        try
        {
            size_t consumed = 0;
            const auto credits = std::stoi(creditsParam, &consumed);
            if (consumed != creditsParam.size())
            {
                throw std::invalid_argument("trailing characters");
            }
            filters.credits = credits;
        }
        catch (const std::exception &)
        {
            callback(errorResponse("credits must be an integer",
                                   drogon::k400BadRequest));
            return;
        }
    }

    const auto instructor = request->getParameter("instructor");
    if (!instructor.empty())
    {
        filters.instructor = instructor;
    }
```

- [ ] **Step 4: `services/Tools.cc`** — in `SearchCoursesTool::execute`, replace the `filters.hasX = ...` assignments the same way:

```cpp
    CourseSearchFilters filters;
    if (args.isObject() && args.isMember("department") &&
        args["department"].isString() && !args["department"].asString().empty())
    {
        filters.department = args["department"].asString();
    }
    bool hasDifficulty = false;
    std::string difficulty;
    if (!tryGetOptionalDifficulty(args, hasDifficulty, difficulty, error))
    {
        callback(toolFailure(error));
        return;
    }
    if (hasDifficulty)
    {
        filters.difficulty = difficulty;
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
```

(Note: `tryGetOptionalDifficulty`'s own `bool hasDifficulty`/`std::string difficulty` out-params are refactored to `std::optional<std::string>` in Task 2b, at which point this call site collapses further — see Task 2b Step 4.)

**Verification:**
```sh
docker compose up --build -d
curl -s -w "\n%{http_code}\n" "http://localhost:8080/courses?department=Computer%20Science&difficulty=hard&credits=3"
curl -s -w "\n%{http_code}\n" "http://localhost:8080/courses"
curl -s -w "\n%{http_code}\n" "http://localhost:8080/courses?difficulty=impossible"
# Expect: identical filtering results and identical 400 for the invalid-difficulty case, matching pre-change behavior
```

---

### Task 2b: `std::optional<T>` for `StudentService`/`Tools.cc`/`StudentsController`/`ValidationHelpers`

**Files:**
- Modify: `services/ValidationHelpers.h` / `.cc` (`tryGetOptionalDifficultyField`)
- Modify: `services/StudentService.h` / `.cc` (`getCourseRecommendations`, `buildSemesterPlan`)
- Modify: `services/Tools.cc` (`tryGetOptionalDifficulty` helper + `GetCourseRecommendationsTool`, `BuildSemesterPlanTool`, `SearchCoursesTool`)
- Modify: `controllers/StudentsController.cc` (`courseRecommendations`, `semesterPlan`)

**Interfaces produced (used by call sites above):**
- `bool ValidationHelpers::tryGetOptionalDifficultyField(const Json::Value&, const std::string&, std::optional<std::string>&, std::string&)`
- `StudentService::getCourseRecommendations(db, studentId, const std::optional<std::string>& preferredDifficulty, maxRecommendations, callback)`
- `StudentService::buildSemesterPlan(db, studentId, const std::optional<std::string>& preferredDifficulty, const std::optional<int64_t>& maxCredits, callback)`

- [ ] **Step 1: `services/ValidationHelpers.h`**

```cpp
#pragma once

#include <json/json.h>

#include <cstdint>
#include <optional>
#include <string>

// Shared parameter-validation glue used by controllers and ToolRegistry so
// the integer-overflow and difficulty-enum checks aren't reimplemented at
// every call site.
namespace ValidationHelpers
{
bool tryGetInt64(const Json::Value &value, int64_t &out, std::string &error);

bool validateDifficultyString(const std::string &difficulty,
                              const std::string &fieldName,
                              std::string &error);

// Looks up `fieldName` in `container` (a JSON object) as an optional
// difficulty-enum string. If the field is absent or null, `value` is set
// to std::nullopt and this returns true (nothing to validate). If
// present, it must be a string that is a valid difficulty value;
// otherwise this returns false with `error` set.
bool tryGetOptionalDifficultyField(const Json::Value &container,
                                   const std::string &fieldName,
                                   std::optional<std::string> &value,
                                   std::string &error);
}  // namespace ValidationHelpers
```

- [ ] **Step 2: `services/ValidationHelpers.cc`** — replace `tryGetOptionalDifficultyField`'s body:

```cpp
bool ValidationHelpers::tryGetOptionalDifficultyField(
    const Json::Value &container,
    const std::string &fieldName,
    std::optional<std::string> &value,
    std::string &error)
{
    value.reset();
    if (!container.isObject() || !container.isMember(fieldName) ||
        container[fieldName].isNull())
    {
        return true;
    }

    const auto &fieldValue = container[fieldName];
    if (!fieldValue.isString() ||
        !validateDifficultyString(fieldValue.asString(), fieldName, error))
    {
        error = fieldName + " must be one of: easy, medium, hard";
        return false;
    }
    value = fieldValue.asString();
    return true;
}
```

- [ ] **Step 3: `services/StudentService.h`** — change the two signatures:

```cpp
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
```

and add `#include <optional>` to the top of the file.

- [ ] **Step 4: `services/StudentService.cc`** — in `getCourseRecommendations`, change the parameter and its 2 capture lists (removing `hasPreferredDifficulty` from both lambda capture lists) and the scoring check:

```cpp
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

            const auto currentGpa =
                students.front()["current_gpa"].as<double>();

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
```

(the rest of the function body — GPA-tier scoring, sorting, response assembly — is unchanged).

Then in `buildSemesterPlan`, change the parameters, both capture lists, the `effectiveMaxCredits` computation (now a clean `value_or`), and the sort comparator:

```cpp
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

            const int64_t effectiveMaxCredits = maxCredits.value_or(
                students.front()["max_weekly_credits"].as<int64_t>());

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
```

(the rest — the credit-limit accumulation loop and response assembly — is unchanged).

- [ ] **Step 5: `services/Tools.cc`** — change `tryGetOptionalDifficulty` to return an optional, and update its 3 call sites:

```cpp
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
```

`GetCourseRecommendationsTool::execute`:

```cpp
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
```

`BuildSemesterPlanTool::execute`:

```cpp
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
```

`SearchCoursesTool::execute` (collapsing the Task 2a placeholder now that the helper itself is optional-based):

```cpp
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
```

Add `#include <optional>` near the top of `Tools.cc`.

- [ ] **Step 6: `controllers/StudentsController.cc`** — `courseRecommendations`:

```cpp
void StudentsController::courseRecommendations(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    int64_t studentId) const
{
    const auto &body = request->getJsonObject();

    std::optional<std::string> preferredDifficulty;
    std::string difficultyError;
    if (body &&
        !ValidationHelpers::tryGetOptionalDifficultyField(
            *body,
            "preferred_difficulty",
            preferredDifficulty,
            difficultyError))
    {
        callback(errorResponse(difficultyError, drogon::k400BadRequest));
        return;
    }

    int64_t maxRecommendations = 3;
    if (body && body->isObject() && body->isMember("max_recommendations") &&
        !(*body)["max_recommendations"].isNull())
    {
        std::string parseError;
        if (!ValidationHelpers::tryGetInt64(
                (*body)["max_recommendations"], maxRecommendations, parseError))
        {
            callback(errorResponse("max_recommendations must be an integer",
                                   drogon::k400BadRequest));
            return;
        }
    }

    StudentService::getCourseRecommendations(
        drogon::app().getDbClient(),
        studentId,
        preferredDifficulty,
        maxRecommendations,
        [callback](ServiceResult result) { callback(toHttpResponse(result)); });
}
```

`semesterPlan`:

```cpp
void StudentsController::semesterPlan(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    int64_t studentId) const
{
    const auto &body = request->getJsonObject();

    std::optional<std::string> preferredDifficulty;
    std::string difficultyError;
    if (body &&
        !ValidationHelpers::tryGetOptionalDifficultyField(
            *body,
            "preferred_difficulty",
            preferredDifficulty,
            difficultyError))
    {
        callback(errorResponse(difficultyError, drogon::k400BadRequest));
        return;
    }

    std::optional<int64_t> maxCredits;
    if (body && body->isObject() && body->isMember("max_credits") &&
        !(*body)["max_credits"].isNull())
    {
        int64_t value = 0;
        std::string parseError;
        if (!ValidationHelpers::tryGetInt64(
                (*body)["max_credits"], value, parseError))
        {
            callback(errorResponse("max_credits must be an integer",
                                   drogon::k400BadRequest));
            return;
        }
        maxCredits = value;
    }

    StudentService::buildSemesterPlan(
        drogon::app().getDbClient(),
        studentId,
        preferredDifficulty,
        maxCredits,
        [callback](ServiceResult result) { callback(toHttpResponse(result)); });
}
```

Add `#include <optional>` near the top of `StudentsController.cc`.

**Verification:**
```sh
docker compose up --build -d

# with the optional params supplied
curl -s -w "\n%{http_code}\n" -X POST http://localhost:8080/students/1/course-recommendations -H "Content-Type: application/json" -d '{"preferred_difficulty": "medium", "max_recommendations": 2}'
curl -s -w "\n%{http_code}\n" -X POST http://localhost:8080/students/1/semester-plan -H "Content-Type: application/json" -d '{"preferred_difficulty": "easy", "max_credits": 12}'

# without them (defaults exercised)
curl -s -w "\n%{http_code}\n" -X POST http://localhost:8080/students/1/course-recommendations -H "Content-Type: application/json" -d '{}'
curl -s -w "\n%{http_code}\n" -X POST http://localhost:8080/students/1/semester-plan -H "Content-Type: application/json" -d '{}'

# invalid difficulty still rejected the same way
curl -s -w "\n%{http_code}\n" -X POST http://localhost:8080/students/1/semester-plan -H "Content-Type: application/json" -d '{"preferred_difficulty": "impossible"}'
```
Compare each body/status against Task 2's pre-change baseline (same test cases against the Task-1-only build) — must match exactly.

---

### Task 3: `toJsonArray` template (replaces 2 manual Result→Json::Value loops)

**Files:**
- Create: `services/JsonHelpers.h`
- Modify: `services/CourseService.cc` (`getCourseDetails` prerequisites loop)
- Modify: `services/StudentService.cc` (`getAvailableCourses` courses loop)

**Interfaces produced:**
- `template <typename Container, typename F> Json::Value toJsonArray(const Container &container, F &&toJson)`

- [ ] **Step 1: Create `services/JsonHelpers.h`**

```cpp
#pragma once

#include <json/json.h>

// Builds a Json::Value array by applying `toJson` to each element of
// `container`, in order. Replaces the repeated
// "Json::Value array(Json::arrayValue); for (...) { array.append(...); }"
// loop pattern used across the service layer.
template <typename Container, typename F>
Json::Value toJsonArray(const Container &container, F &&toJson)
{
    Json::Value array(Json::arrayValue);
    for (const auto &item : container)
    {
        array.append(toJson(item));
    }
    return array;
}
```

- [ ] **Step 2: `services/CourseService.cc`** — replace the prerequisites loop in `getCourseDetails`:

```cpp
            database->execSqlAsync(
                "SELECT prerequisite.code, prerequisite.name, "
                "cp.minimum_grade "
                "FROM course_prerequisites cp "
                "JOIN courses prerequisite "
                "ON prerequisite.id = cp.prerequisite_course_id "
                "WHERE cp.course_id = $1 "
                "ORDER BY prerequisite.id",
                [callback, course = std::move(course)](
                    const drogon::orm::Result &prerequisites) mutable {
                    course["prerequisites"] = toJsonArray(
                        prerequisites,
                        [](const drogon::orm::Row &prerequisite) {
                            Json::Value prerequisiteCourse;
                            prerequisiteCourse["code"] =
                                prerequisite["code"].as<std::string>();
                            prerequisiteCourse["name"] =
                                prerequisite["name"].as<std::string>();
                            prerequisiteCourse["minimum_grade"] =
                                prerequisite["minimum_grade"].as<double>();
                            return prerequisiteCourse;
                        });
                    callback(ServiceResult::ok(std::move(course)));
                },
                [callback](const drogon::orm::DrogonDbException &exception) {
                    LOG_ERROR << "Failed to load course prerequisites: "
                              << exception.base().what();
                    callback(
                        ServiceResult::error("Unable to load course details"));
                },
                courseId);
```

Add `#include "JsonHelpers.h"` near the top of the file.

- [ ] **Step 3: `services/StudentService.cc`** — replace the courses loop in `getAvailableCourses`:

```cpp
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
```

Add `#include "JsonHelpers.h"` near the top of the file.

**Verification:**
```sh
docker compose up --build -d
curl -s -w "\n%{http_code}\n" http://localhost:8080/courses/1/details
curl -s -w "\n%{http_code}\n" http://localhost:8080/students/1/available-courses
# Expect: byte-identical JSON shape/values to pre-Task-3 output for both endpoints
```

If, once in the code, either loop doesn't fit cleanly (e.g. the lambda needs to reference outer captures that fight the generic-lambda-in-template pattern), skip that one call site and note it in the final report rather than forcing it — 1 clean fit still satisfies "at least 2" only if both work, so both must be attempted before deciding this task is done.

---

### Task 4a: Real thread count + configurable DB pool size

**Files:**
- Modify: `config.json`
- Modify: `main.cc`

- [ ] **Step 1: `config.json`** — change `number_of_threads`:

```json
        "number_of_threads": 4,
```

- [ ] **Step 2: `main.cc`** — add a `connectionPoolSize()` helper next to `databasePort()`, and use it in place of the hardcoded `2`:

```cpp
#include <drogon/drogon.h>

#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
std::string envOrDefault(const char *name, const char *defaultValue)
{
    const char *value = std::getenv(name);
    return value != nullptr && value[0] != '\0' ? value : defaultValue;
}

unsigned short databasePort()
{
    const auto portValue = envOrDefault("DB_PORT", "5432");
    const auto port = std::stoul(portValue);
    if (port > std::numeric_limits<unsigned short>::max())
    {
        throw std::out_of_range("DB_PORT must be between 0 and 65535");
    }
    return static_cast<unsigned short>(port);
}

// Matches `number_of_threads` in config.json by default: one DB
// connection per IO thread avoids threads blocking on each other for a
// pooled connection under concurrent load.
std::size_t connectionPoolSize()
{
    const auto poolSizeValue = envOrDefault("DB_POOL_SIZE", "4");
    const auto poolSize = std::stoul(poolSizeValue);
    if (poolSize == 0)
    {
        throw std::out_of_range("DB_POOL_SIZE must be a positive integer");
    }
    return poolSize;
}
}  // namespace

int main()
{
    auto &application = drogon::app();
    application.loadConfigFile("./config.json");

    application.addDbClient(drogon::orm::DbConfig{
        drogon::orm::PostgresConfig{
            envOrDefault("DB_HOST", "db"),
            databasePort(),
            envOrDefault("DB_NAME", "smart_university_advisor"),
            envOrDefault("DB_USER", "advisor"),
            envOrDefault("DB_PASSWORD", "advisor_password"),
            connectionPoolSize(),
            "default",
            false,
            "",
            -1.0,
            false,
            {}}});

    application.run();
    return 0;
}
```

**Verification:**
```sh
docker compose up --build -d
docker compose logs api | grep -i "thread\|pool" | head -20
curl -s -w "\n%{http_code}\n" http://localhost:8080/students/1/profile
# Expect: server starts clean with 4 threads, no crash, normal endpoints still 200
```

---

### Task 4b: Gemini host override (test-only hook) + mock Gemini server

**Files:**
- Modify: `services/GeminiClient.h`
- Modify: `services/GeminiClient.cc`
- Modify: `docker-compose.yml` (add `GEMINI_API_HOST` passthrough, defaulted to the real host — no behavior change unless the env var is set)
- Create: `test/mock_gemini_server.js`

- [ ] **Step 1: `services/GeminiClient.h`** — add a comment noting the new env var; no signature change:

```cpp
// Thin wrapper around the Gemini REST `generateContent` endpoint.
//
// Construct this ONLY inside the /agent/query request handler (lazily),
// never at application startup -- a missing/invalid GEMINI_API_KEY must
// not prevent unrelated endpoints (/courses, /students/...) from working.
// The constructor throws std::runtime_error if GEMINI_API_KEY or
// GEMINI_MODEL is missing/empty; callers must catch that and turn it into
// a clean JSON error response.
//
// GEMINI_API_HOST optionally overrides the API host (default: the real
// Gemini endpoint) -- test-only hook so concurrency/integration tests can
// point this at a local mock server instead of the real internet.
class GeminiClient
{
  public:
    GeminiClient();

    void generateContent(
        const Json::Value &contents,
        const Json::Value &toolDeclarations,
        std::function<void(const Json::Value &)> &&onSuccess,
        std::function<void(const std::string &)> &&onError) const;

  private:
    std::string apiKey_;
    std::string model_;
    std::string apiHost_;
};
```

- [ ] **Step 2: `services/GeminiClient.cc`** — read the host from env (defaulted to the real one) and use it instead of the `constexpr` constant:

```cpp
#include "GeminiClient.h"

#include <cstdlib>
#include <sstream>
#include <stdexcept>

#include <drogon/HttpClient.h>
#include <drogon/drogon.h>

namespace
{
std::string envOrDefault(const char *name, const char *defaultValue)
{
    const char *value = std::getenv(name);
    return value != nullptr && value[0] != '\0' ? value : defaultValue;
}

constexpr const char *kDefaultGeminiHost =
    "https://generativelanguage.googleapis.com";
}  // namespace

GeminiClient::GeminiClient()
    : apiKey_(envOrDefault("GEMINI_API_KEY", "")),
      model_(envOrDefault("GEMINI_MODEL", "")),
      apiHost_(envOrDefault("GEMINI_API_HOST", kDefaultGeminiHost))
{
    if (apiKey_.empty())
    {
        throw std::runtime_error(
            "GEMINI_API_KEY environment variable is missing or empty");
    }
    if (model_.empty())
    {
        throw std::runtime_error(
            "GEMINI_MODEL environment variable is missing or empty");
    }
}

void GeminiClient::generateContent(
    const Json::Value &contents,
    const Json::Value &toolDeclarations,
    std::function<void(const Json::Value &)> &&onSuccess,
    std::function<void(const std::string &)> &&onError) const
{
    Json::Value body;
    body["contents"] = contents;
    if (toolDeclarations.isArray() && !toolDeclarations.empty())
    {
        Json::Value tool;
        tool["functionDeclarations"] = toolDeclarations;
        Json::Value tools(Json::arrayValue);
        tools.append(std::move(tool));
        body["tools"] = std::move(tools);
    }

    auto client = drogon::HttpClient::newHttpClient(apiHost_);
    auto request = drogon::HttpRequest::newHttpJsonRequest(body);
    request->setMethod(drogon::Post);
    std::ostringstream path;
    path << "/v1beta/models/" << model_ << ":generateContent";
    request->setPath(path.str());
    request->setPathEncode(false);
    request->addHeader("x-goog-api-key", apiKey_);

    client->sendRequest(
        request,
        [onSuccess = std::move(onSuccess), onError = std::move(onError)](
            drogon::ReqResult result,
            const drogon::HttpResponsePtr &response) {
            if (result != drogon::ReqResult::Ok || !response)
            {
                onError("Failed to reach the Gemini API (network error)");
                return;
            }

            if (response->getStatusCode() != drogon::k200OK)
            {
                std::ostringstream message;
                message << "Gemini API returned HTTP "
                        << static_cast<int>(response->getStatusCode()) << ": "
                        << response->getBody();
                onError(message.str());
                return;
            }

            const auto responseJson = response->getJsonObject();
            if (!responseJson)
            {
                onError("Gemini API returned a malformed JSON response");
                return;
            }

            onSuccess(*responseJson);
        },
        60.0);
}
```

(Note: `envOrDefault("GEMINI_API_KEY", "")` / `envOrDefault("GEMINI_MODEL", "")` behave identically to the previous `envOrEmpty` — empty default, same empty-check-then-throw logic.)

- [ ] **Step 3: `docker-compose.yml`** — add the new env var to the `api` service, defaulted to the real host:

```yaml
  api:
    build: .
    environment:
      DB_HOST: db
      DB_PORT: 5432
      DB_USER: ${DB_USER:-advisor}
      DB_PASSWORD: ${DB_PASSWORD:-advisor_password}
      DB_NAME: ${DB_NAME:-smart_university_advisor}
      DB_POOL_SIZE: ${DB_POOL_SIZE:-4}
      GEMINI_API_KEY: ${GEMINI_API_KEY:-}
      GEMINI_MODEL: ${GEMINI_MODEL:-gemini-3.1-flash-lite}
      GEMINI_API_HOST: ${GEMINI_API_HOST:-https://generativelanguage.googleapis.com}
```

- [ ] **Step 4: Create `test/mock_gemini_server.js`** — a minimal stand-in for Gemini's `generateContent` endpoint, used only for the concurrency test in Task 4c (not part of the CMake build):

```javascript
// Minimal stand-in for Gemini's `generateContent` endpoint, used only to
// load-test /agent/query's per-request state isolation under real
// concurrency without needing a real GEMINI_API_KEY (none is configured
// in this environment -- see docs/concurrency-test.md). Point
// GeminiClient at this via GEMINI_API_HOST=http://host.docker.internal:<port>.
//
// Behavior: parses the student_id embedded in AgentController's prompt
// text ("...helping student_id N...") and echoes it straight back as the
// model's final answer (no functionCall parts), so each request completes
// in one round trip. A per-request random delay increases the chance that
// concurrent requests genuinely overlap in-flight.
const http = require('http');

const PORT = process.env.MOCK_GEMINI_PORT || 5050;
const MIN_DELAY_MS = Number(process.env.MOCK_GEMINI_MIN_DELAY_MS || 50);
const MAX_DELAY_MS = Number(process.env.MOCK_GEMINI_MAX_DELAY_MS || 250);

function randomDelay()
{
    return MIN_DELAY_MS + Math.random() * (MAX_DELAY_MS - MIN_DELAY_MS);
}

const server = http.createServer((req, res) => {
    if (req.method !== 'POST')
    {
        res.writeHead(404);
        res.end();
        return;
    }

    let body = '';
    req.on('data', (chunk) => { body += chunk; });
    req.on('end', () => {
        let studentId = 'unknown';
        try
        {
            const parsed = JSON.parse(body);
            const text = parsed?.contents?.[0]?.parts?.[0]?.text || '';
            const match = text.match(/helping student_id (\d+)/);
            if (match)
            {
                studentId = match[1];
            }
        }
        catch (error)
        {
            // fall through with studentId = 'unknown'
        }

        setTimeout(() => {
            const response = {
                candidates: [
                    {
                        content: {
                            parts: [{ text: `Echo: ${studentId}` }],
                        },
                    },
                ],
            };
            res.writeHead(200, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify(response));
        }, randomDelay());
    });
});

server.listen(PORT, () => {
    console.log(`Mock Gemini server listening on port ${PORT}`);
});
```

**Verification:**
```sh
node test/mock_gemini_server.js &
curl -s -X POST http://localhost:5050/v1beta/models/test-model:generateContent \
  -H "Content-Type: application/json" \
  -d '{"contents":[{"role":"user","parts":[{"text":"...helping student_id 42..."}]}]}'
# Expect: {"candidates":[{"content":{"parts":[{"text":"Echo: 42"}]}}]}
kill %1

docker compose up --build -d
# Confirm the default (no GEMINI_API_HOST override) still targets the real host and unrelated endpoints are unaffected:
curl -s -w "\n%{http_code}\n" http://localhost:8080/students/1/profile
```

---

### Task 4c: Real load tests + `docs/concurrency-test.md`

**Files:**
- Create: `docs/concurrency-test.md`

- [ ] **Step 1: Enrollment uniqueness race — fire 20+ concurrent identical `POST /enrollments`**

```sh
docker compose up --build -d
mkdir -p /tmp/enroll_race
for i in $(seq 1 25); do
  curl -s -o "/tmp/enroll_race/resp_$i.json" -w "%{http_code}\n" -X POST http://localhost:8080/enrollments \
    -H "Content-Type: application/json" \
    -d '{"student_id": 1, "course_id": 1, "semester": "2027-ConcurrencyTest"}' \
    > "/tmp/enroll_race/status_$i.txt" &
done
wait
cat /tmp/enroll_race/status_*.txt | sort | uniq -c
```
Expect: `1` line reading `201` and `24` lines reading `409` (or whatever split accounts for all 25, but exactly one `201`), no connection resets, no empty responses.

- [ ] **Step 2: Confirm no crash and no duplicate row**

```sh
curl -s -w "\n%{http_code}\n" http://localhost:8080/students/1/profile
# Expect: 200 -- server survived the burst

docker compose exec db psql -U advisor -d smart_university_advisor -c \
  "SELECT count(*) FROM enrollments WHERE student_id = 1 AND course_id = 1 AND semester = '2027-ConcurrencyTest';"
# Expect: exactly 1 row
```

- [ ] **Step 3: Clean up the test row**

```sh
docker compose exec db psql -U advisor -d smart_university_advisor -c \
  "DELETE FROM enrollments WHERE student_id = 1 AND course_id = 1 AND semester = '2027-ConcurrencyTest';"
```

- [ ] **Step 4: `/agent/query` cross-talk test using the mock Gemini server**

```sh
node test/mock_gemini_server.js &
MOCK_PID=$!

docker compose stop api
GEMINI_API_HOST=http://host.docker.internal:5050 GEMINI_API_KEY=test-key GEMINI_MODEL=test-model \
  docker compose up --build -d api

mkdir -p /tmp/agent_concurrency
for sid in $(seq 1 10); do
  curl -s -o "/tmp/agent_concurrency/resp_$sid.json" -X POST http://localhost:8080/agent/query \
    -H "Content-Type: application/json" \
    -d "{\"student_id\": $sid, \"message\": \"test message $sid\"}" &
done
wait

for sid in $(seq 1 10); do
  echo "requested=$sid ->"
  cat "/tmp/agent_concurrency/resp_$sid.json"
  echo
done
```

For each `sid`, confirm the response's `"student_id"` field equals `sid` and `"answer"` equals `"Echo: <sid>"` — i.e. no response contains another request's student_id or echoed text. This is the real test of whether `shared_ptr<AgentState>` isolation holds under genuine parallelism (4 real IO threads, `docker compose up` rebuilt with `number_of_threads: 4`), not a single-threaded callback chain.

```sh
kill $MOCK_PID
docker compose stop api
docker compose up --build -d api
# restore the default (real-host) config for normal use
```

- [ ] **Step 5: Write `docs/concurrency-test.md`** with the exact commands from Steps 1–4 and their real captured output (status-code histogram, the `psql` row count, and the full 10-response cross-talk table), plus the config used (`number_of_threads: 4`, `DB_POOL_SIZE=4`), and an explicit note that the `/agent/query` test used the mock Gemini server (`test/mock_gemini_server.js`) because no real `GEMINI_API_KEY` is configured in this environment. If anything broke (duplicate row, crash, cross-talk, or a `docker compose`/networking failure such as `host.docker.internal` not resolving), document the actual failure and the real output — do not paper over it.

**Verification:** `docs/concurrency-test.md` exists, contains real (not illustrative) command output for every step above, and its config section matches `config.json`/`main.cc` post-Task-4a.

---

## Self-Review

**Spec coverage:**
- Task 1 (P0, polymorphic `Tool` dispatch): covered.
- Task 2 (P1, `std::optional<T>`): covered, split into 2a (`CourseSearchFilters`) and 2b (`StudentService` + the two remaining controllers/`Tools.cc`) since they touch disjoint call sites and can be reviewed/tested independently.
- Task 3 (P2, `toJsonArray` template, optional): covered, with an explicit escape hatch to skip and report if either of the 2 target loops doesn't fit cleanly once in the code.
- Task 4 (P0, real concurrency): covered as 4a (thread/pool config), 4b (the Gemini-host test hook + mock server needed because no real API key exists here), 4c (the actual load tests + `docs/concurrency-test.md`).
- Explicitly-out-of-scope items (`std::variant`, multiple inheritance) are not touched anywhere in this plan.
- The "check for a real rubric first" instruction was followed during planning (see Global Constraints) — none exists in this repo.

**Placeholder scan:** No TBD/"add error handling"/"similar to Task N" placeholders — every step shows the complete code being written, and every verification step has an exact command.

**Type consistency:**
- `Tool::execute`'s signature (`const drogon::orm::DbClientPtr&, const Json::Value&, std::function<void(Json::Value)>&&`) is identical across `Tool.h`, all 8 classes in `Tools.h`/`Tools.cc`, and the call site in `ToolRegistry.cc`.
- `CourseSearchFilters`'s 4 fields are `std::optional<...>` consistently across `CourseService.h`, `CourseService.cc`, `CoursesController.cc`, and `Tools.cc`'s `SearchCoursesTool`.
- `StudentService::getCourseRecommendations`/`buildSemesterPlan`'s new `std::optional` parameters match exactly between `StudentService.h`, `StudentService.cc`, `Tools.cc`, and `StudentsController.cc`.
- `ValidationHelpers::tryGetOptionalDifficultyField`'s new `std::optional<std::string>&` out-param matches between its `.h`/`.cc` and both controller call sites.
- `toJsonArray<Container, F>`'s signature is used identically at both Task 3 call sites (lambda taking `const drogon::orm::Row&`, returning `Json::Value`).

**Known risk flagged in advance:** Task 4c's `/agent/query` cross-talk test depends on `host.docker.internal` resolving from inside the Linux container on Docker Desktop for Windows, which is the default behavior but is environment-dependent — if it doesn't resolve, the plan's own Step 4 instructs documenting that failure honestly in `docs/concurrency-test.md` rather than working around it silently.
