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
    // Drogon's default path encoding percent-encodes ':', but Gemini's
    // "models/{id}:generateContent" path requires the literal colon --
    // without this, Google's front end 404s before the request ever
    // reaches the Gemini API (confirmed with a live call).
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
