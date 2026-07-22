#pragma once

#include "Tool.h"

class GetStudentProfileTool : public Tool
{
  public:
    Json::Value declaration() const override;
    void execute(const drogon::orm::DbClientPtr &database,
                 const Json::Value &args,
                 std::function<void(Json::Value)> &&callback) const override;
};

class GetAcademicSummaryTool : public Tool
{
  public:
    Json::Value declaration() const override;
    void execute(const drogon::orm::DbClientPtr &database,
                 const Json::Value &args,
                 std::function<void(Json::Value)> &&callback) const override;
};

class GetAvailableCoursesTool : public Tool
{
  public:
    Json::Value declaration() const override;
    void execute(const drogon::orm::DbClientPtr &database,
                 const Json::Value &args,
                 std::function<void(Json::Value)> &&callback) const override;
};

class GetCourseRecommendationsTool : public Tool
{
  public:
    Json::Value declaration() const override;
    void execute(const drogon::orm::DbClientPtr &database,
                 const Json::Value &args,
                 std::function<void(Json::Value)> &&callback) const override;
};

class BuildSemesterPlanTool : public Tool
{
  public:
    Json::Value declaration() const override;
    void execute(const drogon::orm::DbClientPtr &database,
                 const Json::Value &args,
                 std::function<void(Json::Value)> &&callback) const override;
};

class AnalyzeAcademicRiskTool : public Tool
{
  public:
    Json::Value declaration() const override;
    void execute(const drogon::orm::DbClientPtr &database,
                 const Json::Value &args,
                 std::function<void(Json::Value)> &&callback) const override;
};

class GetCourseDetailsTool : public Tool
{
  public:
    Json::Value declaration() const override;
    void execute(const drogon::orm::DbClientPtr &database,
                 const Json::Value &args,
                 std::function<void(Json::Value)> &&callback) const override;
};

class SearchCoursesTool : public Tool
{
  public:
    Json::Value declaration() const override;
    void execute(const drogon::orm::DbClientPtr &database,
                 const Json::Value &args,
                 std::function<void(Json::Value)> &&callback) const override;
};
