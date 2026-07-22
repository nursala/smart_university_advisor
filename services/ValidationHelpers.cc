#include "ValidationHelpers.h"

#include "StudentService.h"

bool ValidationHelpers::tryGetInt64(const Json::Value &value,
                                    int64_t &out,
                                    std::string &error)
{
    if (!value.isIntegral() || !value.isInt64())
    {
        error = "value must be an integer";
        return false;
    }
    out = value.asInt64();
    return true;
}

bool ValidationHelpers::validateDifficultyString(const std::string &difficulty,
                                                 const std::string &fieldName,
                                                 std::string &error)
{
    if (!StudentService::isValidDifficulty(difficulty))
    {
        error = fieldName + " must be one of: easy, medium, hard";
        return false;
    }
    return true;
}

bool ValidationHelpers::tryGetOptionalDifficultyField(
    const Json::Value &container,
    const std::string &fieldName,
    bool &hasValue,
    std::string &value,
    std::string &error)
{
    hasValue = false;
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
    hasValue = true;
    return true;
}
