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
// Safely extracts an int64 from `value`. jsoncpp's isIntegral() is true for
// any JSON integer, including unsigned values above INT64_MAX; calling
// asInt64() on one of those throws Json::LogicError. This checks isInt64()
// (which confirms the value actually fits in int64_t) before converting,
// and never throws.
bool tryGetInt64(const Json::Value &value, int64_t &out, std::string &error);

// Validates `difficulty` against the easy/medium/hard enum (via
// StudentService::isValidDifficulty), writing a standard
// "<fieldName> must be one of: easy, medium, hard" message to `error` on
// failure.
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
