DROP TABLE IF EXISTS grades CASCADE;
DROP TABLE IF EXISTS enrollments CASCADE;
DROP TABLE IF EXISTS course_prerequisites CASCADE;
DROP TABLE IF EXISTS courses CASCADE;
DROP TABLE IF EXISTS students CASCADE;
DROP TABLE IF EXISTS instructors CASCADE;
DROP TABLE IF EXISTS users CASCADE;

CREATE TABLE users (
    id BIGSERIAL PRIMARY KEY,
    name VARCHAR(120) NOT NULL,
    email VARCHAR(180) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    role VARCHAR(30) NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT NOW(),

    CONSTRAINT chk_users_role
        CHECK (role IN ('student', 'advisor', 'admin'))
);

CREATE TABLE instructors (
    id BIGSERIAL PRIMARY KEY,
    name VARCHAR(120) NOT NULL,
    email VARCHAR(180) UNIQUE,
    department VARCHAR(120) NOT NULL,
    rating NUMERIC(2,1) DEFAULT 0,

    CONSTRAINT chk_instructors_rating
        CHECK (rating >= 0 AND rating <= 5)
);

CREATE TABLE students (
    id BIGSERIAL PRIMARY KEY,
    user_id BIGINT NOT NULL UNIQUE,
    student_number VARCHAR(50) NOT NULL UNIQUE,
    department VARCHAR(120) NOT NULL,
    year_level INT NOT NULL,
    -- NULL means the student has no official grades yet.
    current_gpa NUMERIC(5,2) DEFAULT NULL,
    max_weekly_credits INT DEFAULT 20,

    CONSTRAINT fk_students_user
        FOREIGN KEY (user_id)
        REFERENCES users(id)
        ON DELETE CASCADE,

    CONSTRAINT chk_students_year_level
        CHECK (year_level BETWEEN 1 AND 6),

    CONSTRAINT chk_students_gpa
        CHECK (current_gpa >= 0 AND current_gpa <= 100),

    CONSTRAINT chk_students_max_weekly_credits
        CHECK (max_weekly_credits > 0 AND max_weekly_credits <= 30)
);

CREATE TABLE courses (
    id BIGSERIAL PRIMARY KEY,
    code VARCHAR(30) NOT NULL UNIQUE,
    name VARCHAR(160) NOT NULL,
    department VARCHAR(120) NOT NULL,
    credits INT NOT NULL,
    difficulty_level VARCHAR(30) NOT NULL,
    estimated_weekly_hours INT NOT NULL,
    description TEXT,
    instructor_id BIGINT,

    CONSTRAINT fk_courses_instructor
        FOREIGN KEY (instructor_id)
        REFERENCES instructors(id)
        ON DELETE SET NULL,

    CONSTRAINT chk_courses_credits
        CHECK (credits > 0 AND credits <= 10),

    CONSTRAINT chk_courses_estimated_weekly_hours
        CHECK (estimated_weekly_hours > 0 AND estimated_weekly_hours <= 30),

    CONSTRAINT chk_courses_difficulty
        CHECK (difficulty_level IN ('easy', 'medium', 'hard'))
);

CREATE TABLE course_prerequisites (
    id BIGSERIAL PRIMARY KEY,
    course_id BIGINT NOT NULL,
    prerequisite_course_id BIGINT NOT NULL,
    minimum_grade NUMERIC(5,2) NOT NULL DEFAULT 60,

    CONSTRAINT fk_course_prerequisites_course
        FOREIGN KEY (course_id)
        REFERENCES courses(id)
        ON DELETE CASCADE,

    CONSTRAINT fk_course_prerequisites_prerequisite_course
        FOREIGN KEY (prerequisite_course_id)
        REFERENCES courses(id)
        ON DELETE CASCADE,

    CONSTRAINT chk_course_prerequisites_minimum_grade
        CHECK (minimum_grade >= 0 AND minimum_grade <= 100),

    CONSTRAINT chk_course_prerequisites_not_same_course
        CHECK (course_id <> prerequisite_course_id),

    CONSTRAINT uq_course_prerequisite_pair
        UNIQUE (course_id, prerequisite_course_id)
);

CREATE TABLE enrollments (
    id BIGSERIAL PRIMARY KEY,
    student_id BIGINT NOT NULL,
    course_id BIGINT NOT NULL,
    semester VARCHAR(30) NOT NULL,
    status VARCHAR(30) NOT NULL,
    enrolled_at TIMESTAMP NOT NULL DEFAULT NOW(),

    CONSTRAINT fk_enrollments_student
        FOREIGN KEY (student_id)
        REFERENCES students(id)
        ON DELETE CASCADE,

    CONSTRAINT fk_enrollments_course
        FOREIGN KEY (course_id)
        REFERENCES courses(id)
        ON DELETE CASCADE,

    CONSTRAINT chk_enrollments_status
        CHECK (status IN ('planned', 'active', 'completed', 'dropped')),

    -- Existing historical seed semesters predate this canonical format.
    -- NOT VALID preserves those rows while enforcing the format for all
    -- newly inserted or updated enrollments.
    CONSTRAINT chk_enrollments_semester_format
        CHECK (semester ~ '^[0-9]{4}-(Spring|Summer|Fall|Winter)$')
        NOT VALID,

    CONSTRAINT uq_student_course_semester
        UNIQUE (student_id, course_id, semester)
);

CREATE TABLE grades (
    id BIGSERIAL PRIMARY KEY,
    enrollment_id BIGINT NOT NULL UNIQUE,
    grade NUMERIC(5,2) NOT NULL,
    passed BOOLEAN NOT NULL,
    graded_at TIMESTAMP NOT NULL DEFAULT NOW(),

    CONSTRAINT fk_grades_enrollment
        FOREIGN KEY (enrollment_id)
        REFERENCES enrollments(id)
        ON DELETE CASCADE,

    CONSTRAINT chk_grades_grade
        CHECK (grade >= 0 AND grade <= 100),

    CONSTRAINT chk_grades_passed_consistency
        CHECK (passed = (grade >= 60))
);

CREATE INDEX idx_students_user_id ON students(user_id);

CREATE INDEX idx_courses_department ON courses(department);
CREATE INDEX idx_courses_difficulty ON courses(difficulty_level);
CREATE INDEX idx_courses_instructor_id ON courses(instructor_id);

CREATE INDEX idx_course_prerequisites_course_id
ON course_prerequisites(course_id);

CREATE INDEX idx_course_prerequisites_prerequisite_course_id
ON course_prerequisites(prerequisite_course_id);

CREATE INDEX idx_enrollments_student_id ON enrollments(student_id);
CREATE INDEX idx_enrollments_course_id ON enrollments(course_id);
CREATE INDEX idx_enrollments_status ON enrollments(status);
CREATE INDEX idx_enrollments_semester ON enrollments(semester);

-- A student can retain any number of historical completed/failed attempts,
-- but can have only one currently planned/active attempt for a course.
CREATE UNIQUE INDEX uq_enrollments_one_open_attempt
ON enrollments(student_id, course_id)
WHERE status IN ('planned', 'active');

CREATE INDEX idx_grades_enrollment_id ON grades(enrollment_id);
