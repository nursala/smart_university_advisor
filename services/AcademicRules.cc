#include "AcademicRules.h"

#include <regex>

bool AcademicRules::isValidSemester(const std::string &semester)
{
    static const std::regex pattern(
        R"(^(20[0-9]{2}|2100)-(Spring|Summer|Fall|Winter)$)");
    return semester.size() <= 30 && std::regex_match(semester, pattern);
}

const char *AcademicRules::semesterFormat()
{
    return "YYYY-Spring, YYYY-Summer, YYYY-Fall, or YYYY-Winter";
}
