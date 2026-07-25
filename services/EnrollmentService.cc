#include "EnrollmentService.h"

#include <drogon/drogon.h>

#include <cmath>
#include <sstream>

#include "AcademicRules.h"

namespace
{
Json::Value enrollmentJson(const drogon::orm::Row &row)
{
    Json::Value value;
    value["id"] = Json::Int64(row["id"].as<int64_t>());
    value["student_id"] = Json::Int64(row["student_id"].as<int64_t>());
    value["course_id"] = Json::Int64(row["course_id"].as<int64_t>());
    value["semester"] = row["semester"].as<std::string>();
    value["status"] = row["status"].as<std::string>();
    value["enrolled_at"] = row["enrolled_at"].as<std::string>();
    return value;
}
}  // namespace

namespace
{
void createInTransaction(
    const drogon::orm::DbClientPtr &database,
    int64_t studentId,
    int64_t courseId,
    const std::string &semester,
    std::function<void(ServiceResult)> &&callback)
{
    if (studentId <= 0 || courseId <= 0)
    {
        callback(ServiceResult::badRequest(
            "student_id and course_id must be positive integers"));
        return;
    }
    if (!AcademicRules::isValidSemester(semester))
    {
        callback(ServiceResult::badRequest(
            std::string("semester must use format ") +
            AcademicRules::semesterFormat()));
        return;
    }

    // Locking the student row serializes credit checks and inserts for one
    // student. The data-modifying CTE makes eligibility + insert one atomic
    // PostgreSQL statement for REST, staff, and agent callers alike.
    database->execSqlAsync(
        "WITH locked_student AS ("
        " SELECT id, max_weekly_credits FROM students WHERE id=$1 FOR UPDATE"
        "), target AS ("
        " SELECT id, code, credits FROM courses WHERE id=$2"
        "), missing AS ("
        " SELECT string_agg(pc.code, ', ' ORDER BY pc.code) AS codes"
        " FROM course_prerequisites cp"
        " JOIN courses pc ON pc.id=cp.prerequisite_course_id"
        " WHERE cp.course_id=$2 AND NOT EXISTS ("
        "  SELECT 1 FROM enrollments e JOIN grades g ON g.enrollment_id=e.id"
        "  WHERE e.student_id=$1 AND e.course_id=cp.prerequisite_course_id"
        "   AND e.status='completed' AND g.passed=TRUE"
        "   AND g.grade>=cp.minimum_grade))"
        ", attempts AS ("
        " SELECT"
        "  COALESCE(bool_or(e.status IN ('planned','active')),FALSE) AS open,"
        "  COALESCE(bool_or(e.status='completed' AND g.passed=TRUE),FALSE)"
        "   AS passed,"
        "  COALESCE(bool_or(e.semester=$3),FALSE) AS same_semester"
        " FROM enrollments e LEFT JOIN grades g ON g.enrollment_id=e.id"
        " WHERE e.student_id=$1 AND e.course_id=$2"
        "), load AS ("
        " SELECT COALESCE(SUM(c.credits),0)::bigint AS credits"
        " FROM enrollments e JOIN courses c ON c.id=e.course_id"
        " WHERE e.student_id=$1 AND e.semester=$3"
        " AND e.status IN ('planned','active'))"
        ", inserted AS ("
        " INSERT INTO enrollments(student_id,course_id,semester,status)"
        " SELECT $1,$2,$3,'planned' FROM locked_student s,target c,"
        "  missing m,attempts a,load l"
        " WHERE m.codes IS NULL AND NOT a.open AND NOT a.passed"
        "  AND NOT a.same_semester"
        "  AND l.credits+c.credits<=s.max_weekly_credits"
        " ON CONFLICT DO NOTHING"
        " RETURNING id,student_id,course_id,semester,status,"
        "  enrolled_at::text AS enrolled_at)"
        " SELECT"
        "  EXISTS(SELECT 1 FROM locked_student) AS student_exists,"
        "  EXISTS(SELECT 1 FROM target) AS course_exists,"
        "  (SELECT codes FROM missing) AS missing_codes,"
        "  (SELECT open FROM attempts) AS open_attempt,"
        "  (SELECT passed FROM attempts) AS passed_attempt,"
        "  (SELECT same_semester FROM attempts) AS same_semester,"
        "  (SELECT credits FROM load) AS current_credits,"
        "  (SELECT credits FROM target) AS added_credits,"
        "  (SELECT max_weekly_credits FROM locked_student) AS max_credits,"
        "  i.id,i.student_id,i.course_id,i.semester,i.status,i.enrolled_at"
        " FROM (VALUES(1)) v(x) LEFT JOIN inserted i ON TRUE",
        [callback](const drogon::orm::Result &result) {
            const auto &row = result.front();
            if (!row["student_exists"].as<bool>())
                return callback(ServiceResult::notFound("Student not found"));
            if (!row["course_exists"].as<bool>())
                return callback(ServiceResult::notFound("Course not found"));
            if (!row["missing_codes"].isNull())
                return callback(ServiceResult::badRequest(
                    "Missing prerequisites: " +
                    row["missing_codes"].as<std::string>()));
            if (row["passed_attempt"].as<bool>())
                return callback(ServiceResult::conflict(
                    "Course was already passed and cannot be retaken"));
            if (row["open_attempt"].as<bool>())
                return callback(ServiceResult::conflict(
                    "Course already has an active or planned enrollment"));
            if (row["same_semester"].as<bool>())
                return callback(ServiceResult::conflict(
                    "Course already has an attempt in this semester"));
            const auto current = row["current_credits"].as<int64_t>();
            const auto added = row["added_credits"].as<int64_t>();
            const auto maximum = row["max_credits"].as<int64_t>();
            if (current + added > maximum)
            {
                std::ostringstream message;
                message << "Credit limit exceeded: current=" << current
                        << ", added=" << added << ", maximum=" << maximum;
                return callback(ServiceResult::badRequest(message.str()));
            }
            if (row["id"].isNull())
                return callback(ServiceResult::conflict(
                    "Enrollment conflicts with an existing attempt"));
            callback(ServiceResult::created(enrollmentJson(row)));
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed to create enrollment: "
                      << exception.base().what();
            callback(ServiceResult::conflict(
                "Enrollment conflicts with current academic records"));
        },
        studentId, courseId, semester);
}
}  // namespace

void EnrollmentService::create(
    const drogon::orm::DbClientPtr &database,
    int64_t studentId,
    int64_t courseId,
    const std::string &semester,
    std::function<void(ServiceResult)> &&callback)
{
    if (studentId <= 0 || courseId <= 0)
    {
        callback(ServiceResult::badRequest(
            "student_id and course_id must be positive integers"));
        return;
    }
    if (!AcademicRules::isValidSemester(semester))
    {
        callback(ServiceResult::badRequest(
            std::string("semester must use format ") +
            AcademicRules::semesterFormat()));
        return;
    }

    database->newTransactionAsync(
        [studentId, courseId, semester, callback](
            const std::shared_ptr<drogon::orm::Transaction> &transaction) {
            if (!transaction)
            {
                callback(ServiceResult::error(
                    "Unable to start enrollment transaction"));
                return;
            }
            transaction->execSqlAsync(
                "SET TRANSACTION ISOLATION LEVEL SERIALIZABLE",
                [transaction, studentId, courseId, semester, callback](
                    const drogon::orm::Result &) {
                    createInTransaction(
                        transaction,
                        studentId,
                        courseId,
                        semester,
                        std::function<void(ServiceResult)>(callback));
                },
                [transaction, callback](
                    const drogon::orm::DrogonDbException &exception) {
                    transaction->rollback();
                    LOG_ERROR << "Failed to configure enrollment transaction: "
                              << exception.base().what();
                    callback(ServiceResult::error(
                        "Unable to create enrollment"));
                });
        });
}

namespace
{
void recordGradeInTransaction(
    const drogon::orm::DbClientPtr &database,
    int64_t enrollmentId,
    double grade,
    std::function<void(ServiceResult)> &&callback)
{
    if (!std::isfinite(grade) || grade < 0 || grade > 100)
    {
        callback(ServiceResult::badRequest(
            "grade must be a finite number between 0 and 100"));
        return;
    }
    const bool passed = grade >= AcademicRules::kPassingGrade;
    database->execSqlAsync(
        "WITH target AS ("
        " SELECT id,student_id FROM enrollments WHERE id=$1 FOR UPDATE"
        "), updated AS ("
        " UPDATE enrollments SET status='completed'"
        " WHERE id IN (SELECT id FROM target) RETURNING id,status"
        "), graded AS ("
        " INSERT INTO grades(enrollment_id,grade,passed)"
        " SELECT id,$2,$3 FROM target"
        " ON CONFLICT(enrollment_id) DO UPDATE SET grade=EXCLUDED.grade,"
        " passed=EXCLUDED.passed,graded_at=NOW()"
        " RETURNING enrollment_id,grade,passed"
        "), recalculated AS ("
        " UPDATE students s SET current_gpa=("
        "  SELECT AVG(x.grade) FROM ("
        "   SELECT g.grade FROM enrollments e JOIN grades g"
        "    ON g.enrollment_id=e.id"
        "   WHERE e.student_id=s.id AND e.status='completed' AND e.id<>$1"
        "   UNION ALL SELECT $2::numeric) x)"
        " WHERE s.id=(SELECT student_id FROM target)"
        " RETURNING current_gpa)"
        " SELECT g.enrollment_id,g.grade,g.passed,u.status,r.current_gpa"
        " FROM graded g CROSS JOIN updated u CROSS JOIN recalculated r",
        [callback](const drogon::orm::Result &result) {
            if (result.empty())
                return callback(ServiceResult::notFound("Enrollment not found"));
            const auto &row = result.front();
            Json::Value value;
            value["enrollment_id"] =
                Json::Int64(row["enrollment_id"].as<int64_t>());
            value["grade"] = row["grade"].as<double>();
            value["passed"] = row["passed"].as<bool>();
            value["status"] = row["status"].as<std::string>();
            value["current_gpa"] =
                std::round(row["current_gpa"].as<double>() * 100.0) / 100.0;
            callback(ServiceResult::ok(std::move(value)));
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed atomic grade/GPA update: "
                      << exception.base().what();
            callback(ServiceResult::error("Unable to record grade"));
        },
        enrollmentId, grade, passed);
}
}  // namespace

void EnrollmentService::recordGrade(
    const drogon::orm::DbClientPtr &database,
    int64_t enrollmentId,
    double grade,
    std::function<void(ServiceResult)> &&callback)
{
    if (!std::isfinite(grade) || grade < 0 || grade > 100)
    {
        callback(ServiceResult::badRequest(
            "grade must be a finite number between 0 and 100"));
        return;
    }
    database->newTransactionAsync(
        [enrollmentId, grade, callback](
            const std::shared_ptr<drogon::orm::Transaction> &transaction) {
            if (!transaction)
                return callback(ServiceResult::error(
                    "Unable to start grade transaction"));
            transaction->execSqlAsync(
                "SET TRANSACTION ISOLATION LEVEL SERIALIZABLE",
                [transaction, enrollmentId, grade, callback](
                    const drogon::orm::Result &) {
                    recordGradeInTransaction(
                        transaction,
                        enrollmentId,
                        grade,
                        std::function<void(ServiceResult)>(callback));
                },
                [transaction, callback](
                    const drogon::orm::DrogonDbException &exception) {
                    transaction->rollback();
                    LOG_ERROR << "Failed to configure grade transaction: "
                              << exception.base().what();
                    callback(ServiceResult::error("Unable to record grade"));
                });
        });
}

namespace
{
void removeInTransaction(
    const drogon::orm::DbClientPtr &database,
    int64_t enrollmentId,
    std::function<void(ServiceResult)> &&callback)
{
    database->execSqlAsync(
        "WITH target AS ("
        " SELECT id,student_id FROM enrollments WHERE id=$1 FOR UPDATE"
        "), deleted AS ("
        " DELETE FROM enrollments WHERE id IN (SELECT id FROM target)"
        " RETURNING id)"
        ", recalculated AS ("
        " UPDATE students s SET current_gpa=("
        "  SELECT AVG(g.grade) FROM enrollments e JOIN grades g"
        "   ON g.enrollment_id=e.id"
        "  WHERE e.student_id=s.id AND e.status='completed' AND e.id<>$1)"
        " WHERE s.id=(SELECT student_id FROM target)"
        " RETURNING current_gpa)"
        " SELECT d.id,r.current_gpa FROM deleted d CROSS JOIN recalculated r",
        [callback](const drogon::orm::Result &result) {
            if (result.empty())
                return callback(ServiceResult::notFound("Enrollment not found"));
            Json::Value value;
            value["id"] = Json::Int64(result.front()["id"].as<int64_t>());
            value["deleted"] = true;
            value["current_gpa"] = result.front()["current_gpa"].isNull()
                                       ? Json::Value(Json::nullValue)
                                       : Json::Value(
                                             std::round(
                                                 result.front()["current_gpa"]
                                                     .as<double>() *
                                                 100.0) /
                                             100.0);
            callback(ServiceResult::ok(std::move(value)));
        },
        [callback](const drogon::orm::DrogonDbException &exception) {
            LOG_ERROR << "Failed atomic enrollment deletion/GPA update: "
                      << exception.base().what();
            callback(ServiceResult::error("Unable to delete enrollment"));
        },
        enrollmentId);
}
}  // namespace

void EnrollmentService::remove(
    const drogon::orm::DbClientPtr &database,
    int64_t enrollmentId,
    std::function<void(ServiceResult)> &&callback)
{
    database->newTransactionAsync(
        [enrollmentId, callback](
            const std::shared_ptr<drogon::orm::Transaction> &transaction) {
            if (!transaction)
                return callback(ServiceResult::error(
                    "Unable to start deletion transaction"));
            transaction->execSqlAsync(
                "SET TRANSACTION ISOLATION LEVEL SERIALIZABLE",
                [transaction, enrollmentId, callback](
                    const drogon::orm::Result &) {
                    removeInTransaction(
                        transaction,
                        enrollmentId,
                        std::function<void(ServiceResult)>(callback));
                },
                [transaction, callback](
                    const drogon::orm::DrogonDbException &exception) {
                    transaction->rollback();
                    LOG_ERROR << "Failed to configure deletion transaction: "
                              << exception.base().what();
                    callback(ServiceResult::error(
                        "Unable to delete enrollment"));
                });
        });
}
