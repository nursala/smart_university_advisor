# Audit Fixes Batch 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the uncaught-exception/integer-overflow bug in `ToolRegistry`/controllers, give `EnrollmentsController` a real service layer, de-duplicate validation glue, and document the schema + API surface in the README.

**Architecture:** A new `services/ValidationHelpers.h/.cc` centralizes the safe-int64 extraction and the difficulty-enum validation glue. A new `services/EnrollmentService.h/.cc` mirrors `StudentService`/`CourseService` and takes over the raw SQL currently inline in `EnrollmentsController.cc`. `ServiceResult`/`ServiceResultHttp` gain `Created`/`Conflict` variants so `EnrollmentsController::create` can return 201/409 through the existing `toHttpResponse` helper. `ToolRegistry::execute` gets a top-level `try/catch` as a backstop.

**Tech Stack:** C++17/20, Drogon, jsoncpp, PostgreSQL, CMake, Docker Compose.

## Global Constraints

- Do not touch: auth/JWT module, bonus frontend, git authorship/commit distribution (explicitly out of scope per the audit spec).
- No behavior change for any currently-passing request: same status codes, same response JSON shape, same error message text, for every case that worked before.
- New source files under `services/` are picked up automatically by `aux_source_directory(services SERVICE_SRC)` in `CMakeLists.txt` — no build file changes needed.
- Verify by actually building and running the stack (`docker compose up --build`) and hitting endpoints with `curl`, not just by reading the code.

---

### Task 1: Safe-integer helper + fix every unsafe `isIntegral()`→`asInt64()`/`asInt()` site + top-level try/catch backstop

**Files:**
- Create: `services/ValidationHelpers.h`, `services/ValidationHelpers.cc`
- Modify: `services/ToolRegistry.cc` (lines 35–57, 353–363, 389–401, 421–437, 479–489, 285–504 wrapped in try/catch)
- Modify: `controllers/StudentsController.cc` (lines 79–90, 126–139, 183–195)
- Modify: `controllers/AgentController.cc` (lines 184–196 — same unsafe pattern, not explicitly named in the audit spec but matches "any other controller doing manual JSON integer extraction")
- Modify: `controllers/EnrollmentsController.cc` (the `isPositiveInteger` helper — same unsafe pattern, folded into the Task 2 rewrite instead of touched twice)

**Interfaces produced (used by Tasks 2 and 3):**
- `bool ValidationHelpers::tryGetInt64(const Json::Value &value, int64_t &out, std::string &error)`
- `bool ValidationHelpers::validateDifficultyString(const std::string &difficulty, const std::string &fieldName, std::string &error)`
- `bool ValidationHelpers::tryGetOptionalDifficultyField(const Json::Value &container, const std::string &fieldName, bool &hasValue, std::string &value, std::string &error)`

- [ ] **Step 1: Write `ValidationHelpers.h`/`.cc`**

`tryGetInt64` checks `isIntegral() && isInt64()` before calling `asInt64()`. `validateDifficultyString` wraps `StudentService::isValidDifficulty` with a standard message. `tryGetOptionalDifficultyField` is the JSON-object-member extraction glue built on top of it (used by `StudentsController`'s two identical blocks).

- [ ] **Step 2: Replace every unsafe call site in `ToolRegistry.cc`** (`tryGetStudentId`, `tryGetCourseId`, `max_recommendations`, `max_credits`, `course_ids[]`, `credits` in `search_courses` — the last one also needs an int32-range check since it calls `.asInt()`, not `.asInt64()`) with `ValidationHelpers::tryGetInt64`, preserving each site's existing error message text.

- [ ] **Step 3: Wrap `ToolRegistry::execute`'s body in `try { ... } catch (const std::exception &ex) { callback(toolFailure(...)); }`** as a defense-in-depth backstop.

- [ ] **Step 4: Fix `StudentsController.cc`** (`max_recommendations`, `max_credits`, `course_ids[]` in `riskAnalysis`) the same way, and fold in the `tryGetOptionalDifficultyField` helper for the two `preferred_difficulty` blocks (this doubles as Task 3 work for this file).

- [ ] **Step 5: Fix `AgentController.cc`**'s `student_id` parsing (line ~185) the same way.

- [ ] **Step 6: Build and reproduce the audit's exact failing cases (before confirmed broken / after confirmed fixed)** — see Verification below.

**Verification:**
```sh
docker compose up --build -d
curl -s -o /dev/null -w "%{http_code}\n" -X POST http://localhost:8080/students/1/course-recommendations \
  -H "Content-Type: application/json" \
  -d '{"max_recommendations": 10000000000000000000}'
# Expect: 400 (was: bare 500 / connection reset before the fix)

curl -s -X POST http://localhost:8080/students/1/course-recommendations \
  -H "Content-Type: application/json" \
  -d '{"max_recommendations": 10000000000000000000}'
# Expect: {"error": "..."} JSON body

curl -s -o /dev/null -w "%{http_code}\n" http://localhost:8080/students/1/profile
# Expect: 200, both before and after — confirms the process didn't crash
```
For the `/agent/query` → `get_course_recommendations` tool-call path: Gemini's actual tool-call arguments can't be forced deterministically from the outside (no `GEMINI_API_KEY` is configured in this environment — `.env.example` ships it empty), so this is verified by directly exercising `ToolRegistry::execute` (the exact function `AgentController`'s async Gemini-response callback invokes) with the overflow value via a `DROGON_TEST` case in `test/test_main.cc`, confirming it returns `{"success": false, "error": "..."}` synchronously without throwing, and that the enclosing test process stays alive. Report this substitution explicitly rather than silently claiming true end-to-end Gemini coverage.

---

### Task 2: `EnrollmentService` + thin `EnrollmentsController`

**Files:**
- Create: `services/EnrollmentService.h`, `services/EnrollmentService.cc`
- Modify: `services/ServiceResult.h` (add `Created`/`Conflict` statuses + factories)
- Modify: `services/ServiceResultHttp.h` (map `Created`→201, `Conflict`→409)
- Modify: `controllers/EnrollmentsController.cc` (rewrite to call the service + `toHttpResponse`)

**Interfaces produced:**
- `EnrollmentService::create(db, studentId, courseId, semester, callback)`
- `EnrollmentService::recordGrade(db, enrollmentId, grade, callback)`
- `EnrollmentService::remove(db, enrollmentId, callback)`
- `ServiceResult::created(Json::Value)`, `ServiceResult::conflict(std::string)`

- [ ] **Step 1: Extend `ServiceResult`/`ServiceResultHttp`** with `Created` (201, carries data like `Ok`) and `Conflict` (409, carries a message like `NotFound`/`BadRequest`). Existing `StudentService`/`CourseService` call sites are unaffected (additive change).

- [ ] **Step 2: Move the 3 handlers' SQL into `EnrollmentService.cc` unchanged** (same queries, same nesting, same conflict handling). Add the code comment on the un-transacted validate→validate→insert sequence noting the `uq_student_course_semester` UNIQUE constraint (and the FKs) are the actual safety net.

- [ ] **Step 3: Rewrite `EnrollmentsController.cc`** to validate input (using `ValidationHelpers::tryGetInt64` for `student_id`/`course_id` per Task 1), call the service, and convert with `toHttpResponse` — same shape as `CoursesController.cc`.

- [ ] **Step 4: Verify end-to-end against the live DB.**

**Verification:**
```sh
# create (201)
curl -s -w "\n%{http_code}\n" -X POST http://localhost:8080/enrollments \
  -H "Content-Type: application/json" \
  -d '{"student_id": 1, "course_id": 1, "semester": "2026-Fall"}'

# duplicate create (409)
curl -s -w "\n%{http_code}\n" -X POST http://localhost:8080/enrollments \
  -H "Content-Type: application/json" \
  -d '{"student_id": 1, "course_id": 1, "semester": "2026-Fall"}'

# bad student (404)
curl -s -w "\n%{http_code}\n" -X POST http://localhost:8080/enrollments \
  -H "Content-Type: application/json" \
  -d '{"student_id": 999999, "course_id": 1, "semester": "2026-Fall"}'

# grade (200) — use the id returned by the create above
curl -s -w "\n%{http_code}\n" -X PATCH http://localhost:8080/enrollments/<id>/grade \
  -H "Content-Type: application/json" -d '{"grade": 88}'

# delete (200) then delete again (404)
curl -s -w "\n%{http_code}\n" -X DELETE http://localhost:8080/enrollments/<id>
curl -s -w "\n%{http_code}\n" -X DELETE http://localhost:8080/enrollments/<id>
```
Compare each response body/status against what the pre-refactor code (read during investigation) would have produced for the same input — they must match.

---

### Task 3: De-duplicate validation glue

**Files:**
- Already created in Task 1: `services/ValidationHelpers.h/.cc`
- Modify: `controllers/CoursesController.cc` (difficulty query-param check, lines 35–46)
- Modify: `controllers/StudentsController.cc` (already updated in Task 1 Step 4 — confirm both `preferred_difficulty` blocks use `tryGetOptionalDifficultyField`)
- Modify: `services/ToolRegistry.cc` (local `tryGetOptionalDifficulty` — change its internal check to call `ValidationHelpers::validateDifficultyString` instead of `StudentService::isValidDifficulty` + a hardcoded message, keeping the exact original message text `"difficulty must be one of: easy, medium, hard"` regardless of which JSON key matched)

- [ ] **Step 1: Update `CoursesController::list`** to call `ValidationHelpers::validateDifficultyString(difficulty, "difficulty", error)`, dropping the now-unused `StudentService.h` include if nothing else in the file needs it.

- [ ] **Step 2: Confirm `StudentsController.cc`'s two blocks and `ToolRegistry.cc`'s local helper all route through `ValidationHelpers`.**

- [ ] **Step 3: Re-run the same difficulty-validation requests from before the change and confirm identical error text/status.**

**Verification:**
```sh
curl -s -w "\n%{http_code}\n" "http://localhost:8080/courses?difficulty=impossible"
# Expect: 400, {"error":"difficulty must be one of: easy, medium, hard"} — same as before

curl -s -w "\n%{http_code}\n" -X POST http://localhost:8080/students/1/course-recommendations \
  -H "Content-Type: application/json" -d '{"preferred_difficulty": "impossible"}'
# Expect: 400, {"error":"preferred_difficulty must be one of: easy, medium, hard"} — same as before
```

---

### Task 4: README schema + endpoint documentation

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Add a Mermaid `erDiagram` block** derived directly from `database/schema.sql`'s 7 tables (`users`, `instructors`, `students`, `courses`, `course_prerequisites`, `enrollments`, `grades`) and their FKs (`students.user_id→users.id`, `courses.instructor_id→instructors.id`, `course_prerequisites.course_id→courses.id`, `course_prerequisites.prerequisite_course_id→courses.id`, `enrollments.student_id→students.id`, `enrollments.course_id→courses.id`, `grades.enrollment_id→enrollments.id`).

- [ ] **Step 2: Add the full 12-endpoint list** (method + path + one-line purpose), derived from the 4 controllers' `METHOD_LIST_BEGIN`/`END` blocks.

- [ ] **Step 3: Add the 8 agent tools list**, derived from `ToolRegistry::toolDeclarations()`.

**Verification:** Manually re-check every table/column/FK in the Mermaid block against `schema.sql`; confirm Mermaid `erDiagram` syntax (entity blocks, relationship cardinality tokens) is valid so it renders on GitHub.

---

## Self-Review

**Spec coverage:** All 4 tasks covered, including the two additional unsafe-parsing sites (`AgentController.cc`, `EnrollmentsController.cc`'s `isPositiveInteger`) found during investigation but not explicitly named in the audit spec — flagged here and will be flagged again in the final report.

**Placeholder scan:** No TBD/"add error handling"/"similar to Task N" — every step names exact files, exact function signatures, and exact verification commands.

**Type consistency:** `ValidationHelpers::tryGetInt64(const Json::Value&, int64_t&, std::string&)` signature is identical everywhere it's called (Tasks 1, 2, 3). `EnrollmentService`'s 3 methods match what `EnrollmentsController.cc` calls in Task 2. `ServiceResult::created`/`conflict` match what `ServiceResultHttp.h`'s switch handles.
