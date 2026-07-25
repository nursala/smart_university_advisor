#pragma once

#include <string>

class AcademicRules
{
  public:
    static constexpr double kPassingGrade = 60.0;
    static bool isValidSemester(const std::string &semester);
    static const char *semesterFormat();
};
