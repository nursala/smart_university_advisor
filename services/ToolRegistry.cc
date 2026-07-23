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
    tools.push_back(std::make_unique<EnrollInCourseTool>());
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
