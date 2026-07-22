#define DROGON_TEST_MAIN
#include <drogon/drogon_test.h>
#include <drogon/drogon.h>

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
