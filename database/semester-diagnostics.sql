\set ON_ERROR_STOP on

\echo 'Noncanonical enrollment rows'
SELECT id, student_id, course_id, semester, status
FROM enrollments
WHERE semester !~ '^(20[0-9]{2}|2100)-(Spring|Summer|Fall|Winter)$'
ORDER BY semester, id;

\echo 'Noncanonical enrollment counts grouped by semester'
SELECT semester, COUNT(*) AS incompatible_row_count
FROM enrollments
WHERE semester !~ '^(20[0-9]{2}|2100)-(Spring|Summer|Fall|Winter)$'
GROUP BY semester
ORDER BY semester;

\echo 'Total noncanonical enrollment rows'
SELECT COUNT(*) AS incompatible_enrollment_count
FROM enrollments
WHERE semester !~ '^(20[0-9]{2}|2100)-(Spring|Summer|Fall|Winter)$';

\echo 'Semester CHECK constraint status'
SELECT
    EXISTS (
        SELECT 1
        FROM pg_constraint
        WHERE conrelid = 'enrollments'::regclass
          AND conname = 'chk_enrollments_semester_format'
    ) AS semester_check_exists;

SELECT
    conname AS constraint_name,
    convalidated AS is_validated,
    pg_get_constraintdef(oid) AS definition
FROM pg_constraint
WHERE conrelid = 'enrollments'::regclass
  AND conname = 'chk_enrollments_semester_format';
