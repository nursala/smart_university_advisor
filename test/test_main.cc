#define DROGON_TEST_MAIN
#include <drogon/drogon_test.h>
#include <drogon/drogon.h>

#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <thread>

#include "../services/JwtService.h"
#include "../services/AuthorizationService.h"
#include "../services/AcademicRules.h"
#include "../services/EnrollmentConfirmationService.h"
#include "../services/EnrollmentService.h"
#include "../services/PasswordHasher.h"
#include "../services/ToolRegistry.h"

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

DROGON_TEST(SemesterValidationUsesCanonicalFormat)
{
    CHECK(AcademicRules::isValidSemester("2026-Fall") == true);
    CHECK(AcademicRules::isValidSemester("2026A") == false);
    CHECK(AcademicRules::isValidSemester("") == false);
    CHECK(AcademicRules::isValidSemester("2026-fall") == false);
}

DROGON_TEST(EnrollmentConfirmationIsSingleUseAndIdentityBound)
{
    const auto proposal = EnrollmentConfirmationService::propose(
        7, 41, 3, "2026-Fall");
    const auto id = proposal["confirmation_id"].asString();
    std::string error;
    CHECK(EnrollmentConfirmationService::consume(id, 8, 41, error)
              .has_value() == false);
    auto confirmed =
        EnrollmentConfirmationService::consume(id, 7, 41, error);
    REQUIRE(confirmed.has_value() == true);
    CHECK((*confirmed)["course_id"].asInt64() == 3);
    CHECK(EnrollmentConfirmationService::consume(id, 7, 41, error)
              .has_value() == false);
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
