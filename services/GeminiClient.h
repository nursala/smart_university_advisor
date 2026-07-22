#pragma once

#include <json/json.h>

#include <functional>
#include <string>

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

    // Sends one conversation turn (the full `contents` array so far) plus
    // the tool declarations to Gemini's generateContent endpoint.
    // onSuccess receives the parsed JSON response body on HTTP 200.
    // onError receives a human-readable message on network failure,
    // non-200 response, or malformed response body -- it never throws or
    // lets an exception escape to the caller.
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
