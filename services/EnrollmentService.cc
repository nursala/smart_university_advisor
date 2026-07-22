#include "EnrollmentService.h"

#include <drogon/drogon.h>

void EnrollmentService::create(
    const drogon::orm::DbClientPtr &database,
    int64_t studentId,
    int64_t courseId,
    const std::string &semester,
    std::function<void(ServiceResult)> &&callback)
{
    // These three statements (validate student, validate course, insert)
    // are not wrapped in a transaction, so in theory the student or course
    // could be deleted between them. That race is harmless here: the
    // uq_student_course_semester UNIQUE constraint on `enrollments` (plus
    // the FK constraints on student_id/course_id) is the actual safety net
    // that prevents a bad row, regardless of what these pre-checks saw.
    database->execSqlAsync(
        "SELECT id FROM students WHERE id = $1",
        [database, callback, studentId, courseId, semester](
            const drogon::orm::Result &students) {
            if (students.empty())
            {
                callback(ServiceResult::notFound("Student not found"));
                return;
            }

            database->execSqlAsync(
                "SELECT id FROM courses WHERE id = $1",
                [database, callback, studentId, courseId, semester](
                    const drogon::orm::Result &courses) {
                    if (courses.empty())
                    {
                        callback(ServiceResult::notFound("Course not found"));
                        return;
                    }

                    database->execSqlAsync(
                        "INSERT INTO enrollments "
                        "(student_id, course_id, semester, status) "
                        "VALUES ($1, $2, $3, 'planned') "
                        "ON CONFLICT (student_id, course_id, semester) "
                        "DO NOTHING "
                        "RETURNING id, student_id, course_id, semester, status, "
                        "enrolled_at::text AS enrolled_at",
                        [callback](const drogon::orm::Result &inserted) {
                            if (inserted.empty())
                            {
                                callback(ServiceResult::conflict(
                                    "Enrollment already exists for this "
                                    "student, course, and semester"));
                                return;
                            }

                            const auto &row = inserted.front();
                            Json::Value enrollment;
                            enrollment["id"] =
                                Json::Int64(row["id"].as<int64_t>());
                            enrollment["student_id"] =
                                Json::Int64(row["student_id"].as<int64_t>());
                            enrollment["course_id"] =
                                Json::Int64(row["course_id"].as<int64_t>());
                            enrollment["semester"] =
                                row["semester"].as<std::string>();
                            enrollment["status"] =
                                row["status"].as<std::string>();
                            enrollment["enrolled_at"] =
                                row["enrolled_at"].as<std::string>();
                            callback(ServiceResult::created(enrollment));
                        },
                        [callback](
                            const drogon::orm::DrogonDbException &exception) {
                            LOG_ERROR << "Failed to create enrollment: "
                                      << exception.base().what();
                            callback(ServiceResult::error(
                                "Unable to create enrollment"));
                        },
                        studentId,
                        courseId,
                        semester);
                },
                [callback](const drogon::orm::DrogonDbException &exception) {
                    LOG_ERROR << "Failed to validate course: "
                              << exception.base().what();
                    callback(
                        ServiceResult::error("Unable to create enrollment"));
                },
                courseId);
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to validate student: " << exception.base().what();
            callback(ServiceResult::error("Unable to create enrollment"));
        },
        studentId);
}

void EnrollmentService::recordGrade(
    const drogon::orm::DbClientPtr &database,
    int64_t enrollmentId,
    double grade,
    std::function<void(ServiceResult)> &&callback)
{
    const auto passed = grade >= 60;
    database->execSqlAsync(
        "WITH updated_enrollment AS ("
        "UPDATE enrollments SET status = 'completed' "
        "WHERE id = $1 RETURNING id, status), "
        "upserted_grade AS ("
        "INSERT INTO grades (enrollment_id, grade, passed) "
        "SELECT id, $2, $3 FROM updated_enrollment "
        "ON CONFLICT (enrollment_id) DO UPDATE "
        "SET grade = EXCLUDED.grade, passed = EXCLUDED.passed, "
        "graded_at = NOW() "
        "RETURNING enrollment_id, grade, passed) "
        "SELECT ug.enrollment_id, ug.grade, ug.passed, ue.status "
        "FROM upserted_grade ug CROSS JOIN updated_enrollment ue",
        [callback](const drogon::orm::Result &updated) {
            if (updated.empty())
            {
                callback(ServiceResult::notFound("Enrollment not found"));
                return;
            }

            const auto &row = updated.front();
            Json::Value result;
            result["enrollment_id"] =
                Json::Int64(row["enrollment_id"].as<int64_t>());
            result["grade"] = row["grade"].as<double>();
            result["passed"] = row["passed"].as<bool>();
            result["status"] = row["status"].as<std::string>();
            callback(ServiceResult::ok(result));
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to record grade: " << exception.base().what();
            callback(ServiceResult::error("Unable to record grade"));
        },
        enrollmentId,
        grade,
        passed);
}

void EnrollmentService::remove(
    const drogon::orm::DbClientPtr &database,
    int64_t enrollmentId,
    std::function<void(ServiceResult)> &&callback)
{
    database->execSqlAsync(
        "DELETE FROM enrollments WHERE id = $1 RETURNING id",
        [callback](const drogon::orm::Result &deleted) {
            if (deleted.empty())
            {
                callback(ServiceResult::notFound("Enrollment not found"));
                return;
            }

            Json::Value result;
            result["id"] = Json::Int64(deleted.front()["id"].as<int64_t>());
            result["deleted"] = true;
            callback(ServiceResult::ok(result));
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to delete enrollment: " << exception.base().what();
            callback(ServiceResult::error("Unable to delete enrollment"));
        },
        enrollmentId);
}
