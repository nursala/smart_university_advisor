#define DROGON_TEST_MAIN
#include <drogon/drogon_test.h>
#include <drogon/drogon.h>

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "../services/AgentLoop.h"
#include "../services/JwtService.h"
#include "../services/AuthorizationService.h"
#include "../services/AcademicRules.h"
#include "../services/EnrollmentService.h"
#include "../services/PasswordHasher.h"
#include "../services/ToolRegistry.h"

namespace
{
Json::Value geminiFunctionCall(
    const std::string &name,
    const Json::Value &arguments = Json::Value(Json::objectValue))
{
    Json::Value call;
    call["name"] = name;
    call["args"] = arguments;
    Json::Value part;
    part["functionCall"] = std::move(call);
    Json::Value parts(Json::arrayValue);
    parts.append(std::move(part));
    Json::Value content;
    content["parts"] = std::move(parts);
    Json::Value candidate;
    candidate["content"] = std::move(content);
    Json::Value candidates(Json::arrayValue);
    candidates.append(std::move(candidate));
    Json::Value response;
    response["candidates"] = std::move(candidates);
    return response;
}

Json::Value geminiText(const std::string &text)
{
    Json::Value part;
    part["text"] = text;
    Json::Value parts(Json::arrayValue);
    parts.append(std::move(part));
    Json::Value content;
    content["parts"] = std::move(parts);
    Json::Value candidate;
    candidate["content"] = std::move(content);
    Json::Value candidates(Json::arrayValue);
    candidates.append(std::move(candidate));
    Json::Value response;
    response["candidates"] = std::move(candidates);
    return response;
}

Json::Value initialConversation()
{
    Json::Value part;
    part["text"] = "test question";
    Json::Value parts(Json::arrayValue);
    parts.append(std::move(part));
    Json::Value turn;
    turn["role"] = "user";
    turn["parts"] = std::move(parts);
    Json::Value contents(Json::arrayValue);
    contents.append(std::move(turn));
    return contents;
}

Json::Value successfulToolResult(const std::string &name)
{
    Json::Value result;
    result["success"] = true;
    result["tool"] = name;
    return result;
}

Json::Value geminiResponseWithContent(Json::Value content)
{
    Json::Value candidate;
    candidate["content"] = std::move(content);
    Json::Value candidates(Json::arrayValue);
    candidates.append(std::move(candidate));
    Json::Value response;
    response["candidates"] = std::move(candidates);
    return response;
}

Json::Value geminiResponseWithPart(Json::Value part)
{
    Json::Value parts(Json::arrayValue);
    parts.append(std::move(part));
    Json::Value content;
    content["parts"] = std::move(parts);
    return geminiResponseWithContent(std::move(content));
}

struct AgentResponseOutcome
{
    bool completed = false;
    bool failed = false;
    bool exceptionEscaped = false;
    std::size_t executeCount = 0;
    std::string error;
    Json::Value toolsUsed{Json::arrayValue};
};

AgentResponseOutcome runSingleAgentResponse(const Json::Value &response)
{
    AgentResponseOutcome outcome;
    try
    {
        AgentLoop::start(
            initialConversation(),
            ToolRegistry::toolDeclarations(),
            [&response](
                const Json::Value &,
                const Json::Value &,
                AgentLoop::ResponseCallback onSuccess,
                AgentLoop::ProviderErrorCallback) {
                onSuccess(response);
            },
            [&outcome](
                const std::string &,
                const Json::Value &,
                AgentLoop::ToolResultCallback) {
                ++outcome.executeCount;
            },
            [&outcome](const std::string &, const Json::Value &used) {
                outcome.completed = true;
                outcome.toolsUsed = used;
            },
            [&outcome](
                const std::string &message,
                const Json::Value &used) {
                outcome.failed = true;
                outcome.error = message;
                outcome.toolsUsed = used;
            });
    }
    catch (...)
    {
        outcome.exceptionEscaped = true;
    }
    return outcome;
}
}  // namespace

DROGON_TEST(BasicTest)
{
    // Add your tests here
}

DROGON_TEST(ToolRegistryRejectsInt64OverflowWithoutThrowing)
{
    // Reproduces the audit's exact failing case: max_recommendations above
    // INT64_MAX (jsoncpp parses/stores it as UInt64, so isIntegral() is
    // true but isInt64() is false) used to make asInt64() throw
    // Json::LogicError, uncaught, inside ToolRegistry::execute -- the
    // same function AgentController's async Gemini-response callback
    // invokes. It must now return a clean {"success": false, ...}
    // instead of throwing, and the callback must still fire.
    Json::Value args;
    args["student_id"] = 1;
    args["max_recommendations"] = Json::Value(Json::UInt64(10000000000000000000ULL));

    bool callbackInvoked = false;
    Json::Value toolResult;
    ToolRegistry::execute(
        nullptr,
        "get_course_recommendations",
        args,
        [&callbackInvoked, &toolResult](Json::Value result) {
            callbackInvoked = true;
            toolResult = std::move(result);
        });

    CHECK(callbackInvoked == true);
    CHECK(toolResult["success"].asBool() == false);
    CHECK(toolResult.isMember("error") == true);
}

DROGON_TEST(PasswordHasherHashIsSaltedAndVerifiesAgainstItself)
{
    const std::string password = "correct horse battery staple";

    const auto hashA = PasswordHasher::hash(password);
    const auto hashB = PasswordHasher::hash(password);

    // Same password, two independent hash() calls -> different salt ->
    // different encoded strings.
    CHECK(hashA != hashB);

    CHECK(PasswordHasher::verify(password, hashA) == true);
    CHECK(PasswordHasher::verify(password, hashB) == true);
}

DROGON_TEST(PasswordHasherRejectsWrongPasswordAndGarbageHash)
{
    const auto hash = PasswordHasher::hash("the-real-password");

    CHECK(PasswordHasher::verify("not-the-real-password", hash) == false);

    // Malformed/unrecognized hash strings must be treated as a non-match,
    // never throw.
    CHECK(PasswordHasher::verify("anything", "") == false);
    CHECK(PasswordHasher::verify("anything", "garbage") == false);
    CHECK(PasswordHasher::verify("anything", "pbkdf2_sha256$notanumber$$") ==
          false);
    CHECK(PasswordHasher::verify(
              "anything", "$2b$12$GxQqY18zWbYkNqO8qH7u/.bqLmVoeI6gqgSAq2m6R5j0I6byQpJ8K") ==
          false);
}

DROGON_TEST(SeededDemoPasswordHashVerifies)
{
    const std::string hash =
        "pbkdf2_sha256$210000$"
        "c21hcnQtdW5pdmVyc2l0eS1kZW1vLXN0dWRlbnQ=$"
        "UOUmwPSL3LAoo4lrAwfWeeMfGyv/yhQxXRReE/QXSOg=";
    CHECK(PasswordHasher::verify("DemoStudent2026!", hash) == true);
}

DROGON_TEST(StudentAuthorizationRejectsAnotherStudent)
{
    AuthenticatedIdentity student{10, "student", 41};
    int64_t authorized = 0;
    std::string error;
    CHECK(AuthorizationService::authorizeStudent(
              student, 42, authorized, error) == false);
    CHECK(AuthorizationService::canRecordGrades(student) == false);
}

DROGON_TEST(StaffAuthorizationAllowsCrossStudentAndGrades)
{
    AuthenticatedIdentity advisor{10, "advisor", std::nullopt};
    int64_t authorized = 0;
    std::string error;
    CHECK(AuthorizationService::authorizeStudent(
              advisor, 42, authorized, error) == true);
    CHECK(authorized == 42);
    CHECK(AuthorizationService::canRecordGrades(advisor) == true);
}

DROGON_TEST(AgentToolArgumentsCannotSwitchStudentIdentity)
{
    Json::Value modelArgs;
    modelArgs["student_id"] = Json::Int64(999);
    modelArgs["course_id"] = Json::Int64(3);
    const auto scoped = ToolRegistry::scopeArguments(modelArgs, 41);
    CHECK(scoped["student_id"].asInt64() == 41);
    CHECK(scoped["course_id"].asInt64() == 3);
}

DROGON_TEST(ToolRegistryContainsExactlyEightReadOnlyTools)
{
    const auto declarations = ToolRegistry::toolDeclarations();
    REQUIRE(declarations.isArray());
    CHECK(declarations.size() == 8);

    const std::vector<std::string> expected{
        "get_student_profile",
        "get_academic_summary",
        "get_available_courses",
        "get_course_recommendations",
        "build_semester_plan",
        "analyze_academic_risk",
        "get_course_details",
        "search_courses"};
    std::vector<std::string> actual;
    for (const auto &declaration : declarations)
        actual.push_back(declaration["name"].asString());
    CHECK(actual == expected);
    CHECK(std::find(actual.begin(), actual.end(), "enroll_in_course") ==
          actual.end());
}

DROGON_TEST(AcademicSummaryToolAdvertisesSeparatedCourseStates)
{
    const auto declarations = ToolRegistry::toolDeclarations();
    REQUIRE(declarations.size() == 8);

    const Json::Value *summaryDeclaration = nullptr;
    for (const auto &declaration : declarations)
    {
        if (declaration["name"].asString() == "get_academic_summary")
        {
            summaryDeclaration = &declaration;
            break;
        }
    }
    REQUIRE(summaryDeclaration != nullptr);
    const auto description =
        (*summaryDeclaration)["description"].asString();
    CHECK(description.find("completed") != std::string::npos);
    CHECK(description.find("active") != std::string::npos);
    CHECK(description.find("planned") != std::string::npos);
}

DROGON_TEST(AgentReportsPlannedCourseWithoutChangingItsAcademicState)
{
    std::size_t requestCount = 0;
    std::size_t executeCount = 0;
    const std::size_t enrollmentRowsBefore = 1;
    std::size_t enrollmentRowsAfter = enrollmentRowsBefore;
    bool identityWasBound = false;
    bool everyRequestHadEightTools = true;
    bool requestedExpectedTool = false;
    bool unexpectedRequest = false;
    bool completed = false;
    bool failed = false;
    std::string answer;
    Json::Value toolsUsed;

    Json::Value modelArgs;
    modelArgs["student_id"] = Json::Int64(999);
    const std::vector<Json::Value> responses{
        geminiFunctionCall("get_academic_summary", modelArgs),
        geminiText(
            "You have CS101 — Introduction to Computer Science planned for "
            "2026-Fall. You currently have no active or completed courses.")};

    AgentLoop::start(
        initialConversation(),
        ToolRegistry::toolDeclarations(),
        [&responses,
         &requestCount,
         &everyRequestHadEightTools,
         &unexpectedRequest](
            const Json::Value &,
            const Json::Value &declarations,
            AgentLoop::ResponseCallback onSuccess,
            AgentLoop::ProviderErrorCallback) {
            everyRequestHadEightTools =
                everyRequestHadEightTools &&
                declarations.size() == 8;
            if (requestCount >= responses.size())
            {
                unexpectedRequest = true;
                return;
            }
            onSuccess(responses[requestCount++]);
        },
        [&executeCount,
         &identityWasBound,
         &requestedExpectedTool,
         &enrollmentRowsAfter](
            const std::string &name,
            const Json::Value &arguments,
            AgentLoop::ToolResultCallback callback) {
            ++executeCount;
            requestedExpectedTool =
                name == "get_academic_summary";
            const auto scoped =
                ToolRegistry::scopeArguments(arguments, 41);
            identityWasBound =
                scoped["student_id"].asInt64() == 41;

            Json::Value plannedCourse;
            plannedCourse["enrollment_id"] = Json::Int64(71);
            plannedCourse["course_id"] = Json::Int64(1);
            plannedCourse["course_code"] = "CS101";
            plannedCourse["course_name"] =
                "Introduction to Computer Science";
            plannedCourse["semester"] = "2026-Fall";
            plannedCourse["credits"] = 4;
            plannedCourse["status"] = "planned";

            Json::Value planned(Json::arrayValue);
            planned.append(std::move(plannedCourse));
            Json::Value data;
            data["completed_courses"] =
                Json::Value(Json::arrayValue);
            data["active_courses"] = Json::Value(Json::arrayValue);
            data["planned_courses"] = std::move(planned);

            Json::Value result;
            result["success"] = true;
            result["data"] = std::move(data);
            callback(std::move(result));
            enrollmentRowsAfter = 1;
        },
        [&completed, &answer, &toolsUsed](
            const std::string &value,
            const Json::Value &used) {
            completed = true;
            answer = value;
            toolsUsed = used;
        },
        [&failed](const std::string &, const Json::Value &) {
            failed = true;
        });

    CHECK(completed == true);
    CHECK(failed == false);
    CHECK(requestCount == 2);
    CHECK(executeCount == 1);
    CHECK(everyRequestHadEightTools == true);
    CHECK(unexpectedRequest == false);
    CHECK(requestedExpectedTool == true);
    CHECK(identityWasBound == true);
    CHECK(enrollmentRowsAfter == enrollmentRowsBefore);
    CHECK(answer.find("CS101") != std::string::npos);
    CHECK(answer.find("2026-Fall") != std::string::npos);
    CHECK(answer.find("planned") != std::string::npos);
    CHECK(answer.find("CS101 is active") == std::string::npos);
    CHECK(answer.find("CS101 is completed") == std::string::npos);
    REQUIRE(toolsUsed.size() == 1);
    CHECK(toolsUsed[0].asString() == "get_academic_summary");
}

DROGON_TEST(AgentLoopCompletesNormalMultiToolChain)
{
    const std::vector<Json::Value> responses{
        geminiFunctionCall("get_student_profile"),
        geminiFunctionCall("get_academic_summary"),
        geminiText("Synthesized answer")};
    std::size_t requestCount = 0;
    std::vector<std::string> executedTools;
    bool completed = false;
    bool failed = false;
    std::string answer;
    Json::Value toolsUsed;
    bool allCallsHadTools = true;
    bool unexpectedRequest = false;

    AgentLoop::start(
        initialConversation(),
        ToolRegistry::toolDeclarations(),
        [&responses, &requestCount, &allCallsHadTools, &unexpectedRequest](
            const Json::Value &,
            const Json::Value &declarations,
            AgentLoop::ResponseCallback onSuccess,
            AgentLoop::ProviderErrorCallback) {
            allCallsHadTools =
                allCallsHadTools && declarations.size() == 8;
            if (requestCount >= responses.size())
            {
                unexpectedRequest = true;
                return;
            }
            onSuccess(responses[requestCount++]);
        },
        [&executedTools](
            const std::string &name,
            const Json::Value &,
            AgentLoop::ToolResultCallback callback) {
            executedTools.push_back(name);
            callback(successfulToolResult(name));
        },
        [&completed, &answer, &toolsUsed](
            const std::string &value,
            const Json::Value &used) {
            completed = true;
            answer = value;
            toolsUsed = used;
        },
        [&failed](const std::string &, const Json::Value &) {
            failed = true;
        });

    CHECK(completed == true);
    CHECK(failed == false);
    CHECK(allCallsHadTools == true);
    CHECK(unexpectedRequest == false);
    CHECK(requestCount == 3);
    REQUIRE(executedTools.size() == 2);
    CHECK(executedTools[0] == "get_student_profile");
    CHECK(executedTools[1] == "get_academic_summary");
    CHECK(answer == "Synthesized answer");
    REQUIRE(toolsUsed.size() == 2);
    CHECK(toolsUsed[0].asString() == executedTools[0]);
    CHECK(toolsUsed[1].asString() == executedTools[1]);
}

DROGON_TEST(AgentLoopReservesFinalSynthesisAfterMaximumToolRounds)
{
    std::size_t requestCount = 0;
    std::vector<std::string> executedTools;
    bool completed = false;
    bool failed = false;
    std::string answer;
    Json::Value toolsUsed;
    bool finalCallHadNoTools = false;
    bool finalCallContainedToolResults = false;
    bool normalCallsHadTools = true;

    AgentLoop::start(
        initialConversation(),
        ToolRegistry::toolDeclarations(),
        [&requestCount,
         &finalCallHadNoTools,
         &finalCallContainedToolResults,
         &normalCallsHadTools](
            const Json::Value &contents,
            const Json::Value &declarations,
            AgentLoop::ResponseCallback onSuccess,
            AgentLoop::ProviderErrorCallback) {
            const auto callIndex = requestCount++;
            if (callIndex < AgentLoop::kDefaultMaxToolRounds)
            {
                normalCallsHadTools =
                    normalCallsHadTools && declarations.size() == 8;
                onSuccess(geminiFunctionCall(
                    "get_student_profile"));
                return;
            }

            finalCallHadNoTools = declarations.empty();
            std::size_t functionResponseCount = 0;
            for (const auto &turn : contents)
            {
                if (!turn.isObject() || !turn["parts"].isArray())
                    continue;
                for (const auto &part : turn["parts"])
                {
                    if (part.isObject() &&
                        part.isMember("functionResponse"))
                    {
                        ++functionResponseCount;
                    }
                }
            }
            finalCallContainedToolResults =
                functionResponseCount ==
                AgentLoop::kDefaultMaxToolRounds;
            onSuccess(geminiText("Final answer from collected results"));
        },
        [&executedTools](
            const std::string &name,
            const Json::Value &,
            AgentLoop::ToolResultCallback callback) {
            executedTools.push_back(name);
            callback(successfulToolResult(name));
        },
        [&completed, &answer, &toolsUsed](
            const std::string &value,
            const Json::Value &used) {
            completed = true;
            answer = value;
            toolsUsed = used;
        },
        [&failed](const std::string &, const Json::Value &) {
            failed = true;
        });

    CHECK(completed == true);
    CHECK(failed == false);
    CHECK(normalCallsHadTools == true);
    CHECK(requestCount == AgentLoop::kDefaultMaxToolRounds + 1);
    CHECK(executedTools.size() == AgentLoop::kDefaultMaxToolRounds);
    CHECK(toolsUsed.size() == executedTools.size());
    CHECK(finalCallHadNoTools == true);
    CHECK(finalCallContainedToolResults == true);
    CHECK(answer == "Final answer from collected results");
}

DROGON_TEST(AgentLoopDoesNotExecuteToolRequestedDuringFinalSynthesis)
{
    std::size_t requestCount = 0;
    std::size_t executeCount = 0;
    bool completed = false;
    bool failed = false;
    Json::Value errorToolsUsed;
    bool normalCallsHadTools = true;
    bool finalCallHadNoTools = false;

    AgentLoop::start(
        initialConversation(),
        ToolRegistry::toolDeclarations(),
        [&requestCount, &normalCallsHadTools, &finalCallHadNoTools](
            const Json::Value &,
            const Json::Value &declarations,
            AgentLoop::ResponseCallback onSuccess,
            AgentLoop::ProviderErrorCallback) {
            const auto callIndex = requestCount++;
            if (callIndex < AgentLoop::kDefaultMaxToolRounds)
            {
                normalCallsHadTools =
                    normalCallsHadTools && declarations.size() == 8;
                onSuccess(geminiFunctionCall("search_courses"));
                return;
            }
            finalCallHadNoTools = declarations.empty();
            onSuccess(geminiFunctionCall("get_course_details"));
        },
        [&executeCount](
            const std::string &name,
            const Json::Value &,
            AgentLoop::ToolResultCallback callback) {
            ++executeCount;
            callback(successfulToolResult(name));
        },
        [&completed](const std::string &, const Json::Value &) {
            completed = true;
        },
        [&failed, &errorToolsUsed](
            const std::string &,
            const Json::Value &used) {
            failed = true;
            errorToolsUsed = used;
        });

    CHECK(completed == false);
    CHECK(failed == true);
    CHECK(normalCallsHadTools == true);
    CHECK(finalCallHadNoTools == true);
    CHECK(requestCount == AgentLoop::kDefaultMaxToolRounds + 1);
    CHECK(executeCount == AgentLoop::kDefaultMaxToolRounds);
    CHECK(errorToolsUsed.size() == executeCount);
}

DROGON_TEST(AgentLoopEarlyFinalAnswerUsesSingleGeminiCall)
{
    std::size_t requestCount = 0;
    std::size_t executeCount = 0;
    bool completed = false;
    bool failed = false;
    Json::Value toolsUsed;
    bool requestHadTools = false;

    AgentLoop::start(
        initialConversation(),
        ToolRegistry::toolDeclarations(),
        [&requestCount, &requestHadTools](
            const Json::Value &,
            const Json::Value &declarations,
            AgentLoop::ResponseCallback onSuccess,
            AgentLoop::ProviderErrorCallback) {
            ++requestCount;
            requestHadTools = declarations.size() == 8;
            onSuccess(geminiText("Immediate answer"));
        },
        [&executeCount](
            const std::string &,
            const Json::Value &,
            AgentLoop::ToolResultCallback) {
            ++executeCount;
        },
        [&completed, &toolsUsed](
            const std::string &,
            const Json::Value &used) {
            completed = true;
            toolsUsed = used;
        },
        [&failed](const std::string &, const Json::Value &) {
            failed = true;
        });

    CHECK(completed == true);
    CHECK(failed == false);
    CHECK(requestHadTools == true);
    CHECK(requestCount == 1);
    CHECK(executeCount == 0);
    CHECK(toolsUsed.empty());
}

DROGON_TEST(AgentLoopFinalSynthesisProviderFailureUsesErrorPath)
{
    std::size_t requestCount = 0;
    std::size_t executeCount = 0;
    bool completed = false;
    bool failed = false;
    std::string errorMessage;
    Json::Value errorToolsUsed;

    AgentLoop::start(
        initialConversation(),
        ToolRegistry::toolDeclarations(),
        [&requestCount](
            const Json::Value &,
            const Json::Value &declarations,
            AgentLoop::ResponseCallback onSuccess,
            AgentLoop::ProviderErrorCallback onError) {
            const auto callIndex = requestCount++;
            if (callIndex == 0)
            {
                onSuccess(geminiFunctionCall("get_student_profile"));
                return;
            }
            if (declarations.empty())
                onError("synthetic provider failure");
        },
        [&executeCount](
            const std::string &name,
            const Json::Value &,
            AgentLoop::ToolResultCallback callback) {
            ++executeCount;
            callback(successfulToolResult(name));
        },
        [&completed](const std::string &, const Json::Value &) {
            completed = true;
        },
        [&failed, &errorMessage, &errorToolsUsed](
            const std::string &message,
            const Json::Value &used) {
            failed = true;
            errorMessage = message;
            errorToolsUsed = used;
        },
        1);

    CHECK(completed == false);
    CHECK(failed == true);
    CHECK(requestCount == 2);
    CHECK(executeCount == 1);
    CHECK(errorMessage == "synthetic provider failure");
    REQUIRE(errorToolsUsed.size() == 1);
    CHECK(errorToolsUsed[0].asString() == "get_student_profile");
}

DROGON_TEST(AgentLoopRejectsNonObjectResponseRoot)
{
    const auto outcome =
        runSingleAgentResponse(Json::Value("not an object"));
    CHECK(outcome.failed == true);
    CHECK(outcome.completed == false);
    CHECK(outcome.executeCount == 0);
    CHECK(outcome.toolsUsed.empty());
    CHECK(outcome.exceptionEscaped == false);
    CHECK(outcome.error == "Gemini API returned a malformed response");
}

DROGON_TEST(AgentLoopRejectsMissingWrongOrEmptyCandidates)
{
    Json::Value missingCandidates(Json::objectValue);
    Json::Value wrongCandidates;
    wrongCandidates["candidates"] = 7;
    Json::Value emptyCandidates;
    emptyCandidates["candidates"] = Json::Value(Json::arrayValue);

    for (const auto &response :
         {missingCandidates, wrongCandidates, emptyCandidates})
    {
        const auto outcome = runSingleAgentResponse(response);
        CHECK(outcome.failed == true);
        CHECK(outcome.completed == false);
        CHECK(outcome.executeCount == 0);
        CHECK(outcome.toolsUsed.empty());
        CHECK(outcome.exceptionEscaped == false);
    }
}

DROGON_TEST(AgentLoopRejectsMalformedContentAndParts)
{
    const auto malformedContent =
        geminiResponseWithContent(Json::Value("not an object"));

    Json::Value contentWithWrongParts;
    contentWithWrongParts["parts"] = Json::Value(Json::objectValue);
    const auto wrongParts =
        geminiResponseWithContent(std::move(contentWithWrongParts));

    const auto nonObjectPart =
        geminiResponseWithPart(Json::Value("not an object"));

    for (const auto &response :
         {malformedContent, wrongParts, nonObjectPart})
    {
        const auto outcome = runSingleAgentResponse(response);
        CHECK(outcome.failed == true);
        CHECK(outcome.completed == false);
        CHECK(outcome.executeCount == 0);
        CHECK(outcome.toolsUsed.empty());
        CHECK(outcome.exceptionEscaped == false);
        CHECK(outcome.error ==
              "Gemini API returned a malformed response");
    }
}

DROGON_TEST(AgentLoopRejectsNonStringText)
{
    Json::Value part;
    part["text"] = 42;
    const auto outcome =
        runSingleAgentResponse(geminiResponseWithPart(std::move(part)));

    CHECK(outcome.failed == true);
    CHECK(outcome.completed == false);
    CHECK(outcome.executeCount == 0);
    CHECK(outcome.toolsUsed.empty());
    CHECK(outcome.exceptionEscaped == false);
}

DROGON_TEST(AgentLoopRejectsNonObjectFunctionCall)
{
    Json::Value part;
    part["functionCall"] = "not an object";
    const auto outcome =
        runSingleAgentResponse(geminiResponseWithPart(std::move(part)));

    CHECK(outcome.failed == true);
    CHECK(outcome.completed == false);
    CHECK(outcome.executeCount == 0);
    CHECK(outcome.toolsUsed.empty());
    CHECK(outcome.exceptionEscaped == false);
}

DROGON_TEST(AgentLoopRejectsMissingEmptyOrNonStringToolName)
{
    Json::Value missingNameCall(Json::objectValue);
    missingNameCall["args"] = Json::Value(Json::objectValue);
    Json::Value missingNamePart;
    missingNamePart["functionCall"] = std::move(missingNameCall);

    Json::Value emptyNameCall;
    emptyNameCall["name"] = "   ";
    emptyNameCall["args"] = Json::Value(Json::objectValue);
    Json::Value emptyNamePart;
    emptyNamePart["functionCall"] = std::move(emptyNameCall);

    Json::Value wrongNameCall;
    wrongNameCall["name"] = 9;
    wrongNameCall["args"] = Json::Value(Json::objectValue);
    Json::Value wrongNamePart;
    wrongNamePart["functionCall"] = std::move(wrongNameCall);

    for (auto response :
         {geminiResponseWithPart(std::move(missingNamePart)),
          geminiResponseWithPart(std::move(emptyNamePart)),
          geminiResponseWithPart(std::move(wrongNamePart))})
    {
        const auto outcome = runSingleAgentResponse(response);
        CHECK(outcome.failed == true);
        CHECK(outcome.completed == false);
        CHECK(outcome.executeCount == 0);
        CHECK(outcome.toolsUsed.empty());
        CHECK(outcome.exceptionEscaped == false);
    }
}

DROGON_TEST(AgentLoopRejectsWrongArgumentType)
{
    Json::Value call;
    call["name"] = "search_courses";
    call["args"] = Json::Value(Json::arrayValue);
    Json::Value part;
    part["functionCall"] = std::move(call);
    const auto outcome =
        runSingleAgentResponse(geminiResponseWithPart(std::move(part)));

    CHECK(outcome.failed == true);
    CHECK(outcome.completed == false);
    CHECK(outcome.executeCount == 0);
    CHECK(outcome.toolsUsed.empty());
    CHECK(outcome.exceptionEscaped == false);
}

DROGON_TEST(AgentLoopAllowsOmittedArgsOnlyForToolWithoutRequiredFields)
{
    std::size_t requestCount = 0;
    std::size_t executeCount = 0;
    bool argumentsWereEmptyObject = false;
    bool completed = false;
    bool failed = false;
    Json::Value toolsUsed;

    AgentLoop::start(
        initialConversation(),
        ToolRegistry::toolDeclarations(),
        [&requestCount](
            const Json::Value &,
            const Json::Value &,
            AgentLoop::ResponseCallback onSuccess,
            AgentLoop::ProviderErrorCallback) {
            if (requestCount++ == 0)
            {
                Json::Value call;
                call["name"] = "search_courses";
                Json::Value part;
                part["functionCall"] = std::move(call);
                onSuccess(
                    geminiResponseWithPart(std::move(part)));
                return;
            }
            onSuccess(geminiText("Catalog search complete"));
        },
        [&executeCount, &argumentsWereEmptyObject](
            const std::string &,
            const Json::Value &arguments,
            AgentLoop::ToolResultCallback callback) {
            ++executeCount;
            argumentsWereEmptyObject =
                arguments.isObject() && arguments.empty();
            callback(successfulToolResult("search_courses"));
        },
        [&completed, &toolsUsed](
            const std::string &,
            const Json::Value &used) {
            completed = true;
            toolsUsed = used;
        },
        [&failed](const std::string &, const Json::Value &) {
            failed = true;
        });

    CHECK(completed == true);
    CHECK(failed == false);
    CHECK(requestCount == 2);
    CHECK(executeCount == 1);
    CHECK(argumentsWereEmptyObject == true);
    REQUIRE(toolsUsed.size() == 1);
    CHECK(toolsUsed[0].asString() == "search_courses");
}

DROGON_TEST(AgentLoopRejectsOmittedArgsForToolWithRequiredFields)
{
    Json::Value call;
    call["name"] = "get_course_details";
    Json::Value part;
    part["functionCall"] = std::move(call);
    const auto outcome =
        runSingleAgentResponse(geminiResponseWithPart(std::move(part)));

    CHECK(outcome.failed == true);
    CHECK(outcome.completed == false);
    CHECK(outcome.executeCount == 0);
    CHECK(outcome.toolsUsed.empty());
    CHECK(outcome.exceptionEscaped == false);
}

DROGON_TEST(AgentLoopUsesLaterValidCandidate)
{
    Json::Value malformedCandidate;
    malformedCandidate["content"] = "bad";
    Json::Value validCandidate;
    validCandidate["content"] =
        geminiText("unused")["candidates"][0]["content"];
    Json::Value candidates(Json::arrayValue);
    candidates.append(std::move(malformedCandidate));
    candidates.append(std::move(validCandidate));
    Json::Value response;
    response["candidates"] = std::move(candidates);

    const auto outcome = runSingleAgentResponse(response);
    CHECK(outcome.completed == true);
    CHECK(outcome.failed == false);
    CHECK(outcome.executeCount == 0);
    CHECK(outcome.exceptionEscaped == false);
}

DROGON_TEST(SemesterValidationUsesCanonicalFormat)
{
    CHECK(AcademicRules::isValidSemester("2000-Spring") == true);
    CHECK(AcademicRules::isValidSemester("2100-Winter") == true);
    CHECK(AcademicRules::isValidSemester("2028-Fall") == true);
    CHECK(AcademicRules::isValidSemester("1999-Fall") == false);
    CHECK(AcademicRules::isValidSemester("2101-Fall") == false);
    CHECK(AcademicRules::isValidSemester("0000-Fall") == false);
    CHECK(AcademicRules::isValidSemester("20A8-Fall") == false);
    CHECK(AcademicRules::isValidSemester("2028-Autumn") == false);
    CHECK(AcademicRules::isValidSemester("2028-fall") == false);
    CHECK(AcademicRules::isValidSemester(" 2028-Fall") == false);
    CHECK(AcademicRules::isValidSemester("2028-Fall ") == false);
    CHECK(AcademicRules::isValidSemester("") == false);
}

DROGON_TEST(EnrollmentServiceRejectsInvalidInputsBeforeDatabaseAccess)
{
    bool called = false;
    ServiceResult result;
    EnrollmentService::create(
        nullptr, 1, 1, "2026A",
        [&called, &result](ServiceResult value) {
            called = true;
            result = std::move(value);
        });
    CHECK(called == true);
    CHECK(result.status == ServiceResult::Status::BadRequest);

    called = false;
    EnrollmentService::recordGrade(
        nullptr, 1, std::numeric_limits<double>::infinity(),
        [&called, &result](ServiceResult value) {
            called = true;
            result = std::move(value);
        });
    CHECK(called == true);
    CHECK(result.status == ServiceResult::Status::BadRequest);
}

namespace
{
// JwtService reads JWT_SECRET from the environment at construction time.
// The test container already has it set (docker-compose.yml), so the
// round-trip/tamper/expiry tests below run against whatever real value is
// configured -- only the "missing secret" test below needs to temporarily
// clear it.
struct ScopedEnvVar
{
    std::string name;
    std::optional<std::string> previousValue;

    explicit ScopedEnvVar(std::string envName) : name(std::move(envName))
    {
        const char *current = std::getenv(name.c_str());
        if (current != nullptr)
        {
            previousValue = current;
        }
    }

    void unset()
    {
        unsetenv(name.c_str());
    }

    ~ScopedEnvVar()
    {
        if (previousValue.has_value())
        {
            setenv(name.c_str(), previousValue->c_str(), 1);
        }
        else
        {
            unsetenv(name.c_str());
        }
    }
};
}  // namespace

DROGON_TEST(JwtServiceIssueThenVerifyRoundTrips)
{
    JwtService jwtService;
    const auto token = jwtService.issue(42, "student");

    const auto claims = jwtService.verify(token);
    REQUIRE(claims.has_value() == true);
    CHECK(claims->userId == 42);
    CHECK(claims->role == "student");
}

DROGON_TEST(JwtServiceRejectsTamperedSignature)
{
    JwtService jwtService;
    auto token = jwtService.issue(7, "admin");

    // Flip one character in the signature segment (after the last '.').
    const auto lastDot = token.find_last_of('.');
    REQUIRE(lastDot != std::string::npos);
    char &tamperedChar = token[token.size() - 1];
    tamperedChar = (tamperedChar == 'A') ? 'B' : 'A';

    const auto claims = jwtService.verify(token);
    CHECK(claims.has_value() == false);
}

DROGON_TEST(JwtServiceRejectsExpiredToken)
{
    JwtService jwtService;
    // Negative TTL -> exp is already in the past the instant it's issued.
    const auto token = jwtService.issue(1, "student", /*ttlSeconds=*/-1);

    const auto claims = jwtService.verify(token);
    CHECK(claims.has_value() == false);
}

DROGON_TEST(JwtServiceRejectsTokenFromPreviousBoot)
{
    JwtService previousBoot("previous-test-boot");
    JwtService currentBoot("current-test-boot");
    const auto oldToken = previousBoot.issue(19, "advisor");
    CHECK(previousBoot.verify(oldToken).has_value());
    CHECK(currentBoot.verify(oldToken).has_value() == false);
    const auto newToken = currentBoot.issue(19, "advisor");
    const auto claims = currentBoot.verify(newToken);
    REQUIRE(claims.has_value());
    CHECK(claims->userId == 19);
    CHECK(claims->role == "advisor");
}

DROGON_TEST(JwtServiceRejectsTokenWithoutBootClaim)
{
    JwtService jwtService("boot-claim-test");
    const auto legacy =
        jwtService.issueWithoutBootClaimForTesting(5, "student");
    CHECK(jwtService.verify(legacy).has_value() == false);
}

DROGON_TEST(JwtServiceConstructorThrowsWithoutSecret)
{
    ScopedEnvVar scopedSecret("JWT_SECRET");
    scopedSecret.unset();

    CHECK_THROWS_AS(JwtService(), std::runtime_error);
    // scopedSecret's destructor restores JWT_SECRET to its prior value here,
    // so every test above/below this one is unaffected.
}

int main(int argc, char** argv)
{
    using namespace drogon;

    std::promise<void> p1;
    std::future<void> f1 = p1.get_future();

    // Start the main loop on another thread
    std::thread thr([&]() {
        // Queues the promise to be fulfilled after starting the loop
        app().getLoop()->queueInLoop([&p1]() { p1.set_value(); });
        app().run();
    });

    // The future is only satisfied after the event loop started
    f1.get();
    int status = test::run(argc, argv);

    // Ask the event loop to shutdown and wait
    app().getLoop()->queueInLoop([]() { app().quit(); });
    thr.join();
    return status;
}
