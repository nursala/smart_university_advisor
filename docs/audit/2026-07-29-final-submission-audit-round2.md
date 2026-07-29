# Final submission audit — round 2

**Date:** 2026-07-29
**Branch:** `main` @ `718c47c`
**Scope:** Full re-audit after the four post-2026-07-28 merges. Every claim below is
backed by a command run or a file read during this audit session. Read-only: no source
file was modified.

**Machine note:** Windows 11 Pro, Docker 29.3.1 / Compose v5.1.0, host `psql` 15.
Findings that are host-dependent are flagged as such.

---

## 0. Headline verdict

**The repository is ready to submit as-is.** All seven verification phases pass. No
blocking defect was found, and all four prior fixes hold up under fresh verification.
Three cosmetic observations are recorded in §9; none require a follow-up task before
submission.

---

## 1. Status table

| # | Item | Status | Evidence |
|---|---|---|---|
| 0.1 | Working tree clean, synced with origin | ✅ | `git status --short` → empty; `git rev-list --left-right --count origin/main...main` → `0  0` |
| 0.2 | True clean-slate boot (`down --volumes`) | ✅ | Run **twice** this session. Cold: 119 s (85 s compile + 5 s export + ~15 s start). Warm-cache re-run: 11 s. Volume `CreatedAt 2026-07-29T08:52:27Z` |
| 0.3 | Single documented command brings up whole stack | ✅ | `docker compose up --build -d` → `db (healthy)`, `api`, `frontend` all `Up`; ports 8080 / 5433 / 5173 |
| 0.4 | Demo logins from genuinely fresh volume | ✅ | `adam@example.com` → **200** + token (`role: student`, `student_id: 1`); `admin@example.com` → **200** + token (`role: admin`). Re-confirmed after the second reseed |
| 0.5 | Full test suite | ✅ | `All tests passed (199 assertions in 34 tests cases).` — run twice, both times **34 cases / 199 assertions / 0 failures** |
| 1.1 | ≥8 function tools, no near-duplicates | ✅ | `ToolRegistry.cc:16-23` — exactly 8 `push_back`s, 8 semantically distinct actions |
| 1.2 | Tools are read-only | ✅ | `Tools.cc` reaches only 8 `get*/search*/analyze*/build*` service methods; **zero** references to `EnrollmentService::` or `UserService::` (where all 5 mutating methods live) |
| 1.3 | Live Gemini multi-step chain (≥3 tools) | ✅ | Live run this session: `tools_used: ["get_student_profile","get_academic_summary","build_semester_plan"]` — 3 tools, `status: ok` |
| 1.4 | `docs/agent-demo.md` present, labeled, internally consistent | ✅ | "Evidence type: **real Gemini**" vs "Evidence type: **mocked Gemini**" clearly separated; all 4 named tools exist in `ToolRegistry`; transcript's GPA 86.5 / 4 completed / CS302 active / 16 credits match live DB exactly |
| 1.5 | int64-overflow edge case | ✅ | `student_id: 99999999999999999999` → **400** `{"error":"student_id must be an integer"}` |
| 1.6 | Adjacent agent edge cases | ✅ | float id → 400; string id → 400; empty message → 400; no auth → 401; cross-student → 403; "enroll me in CS301" → 200 with `tools_used: []` and refusal text |
| 2.1 | Endpoint count still 17 | ✅ | 17 `ADD_METHOD_TO` across 6 controller headers (1+2+2+4+6+2); 17 extracted route paths |
| 2.2 | README endpoint table matches code exactly | ✅ | All 17 method+path pairs matched 1:1 against `ADD_METHOD_TO` declarations |
| 2.3 | `EnrollmentsController::remove` free of raw SQL | ✅ | Now calls `EnrollmentService::findForRemoval` → authorize → `EnrollmentService::remove` (`EnrollmentsController.cc:149-190`) |
| 2.4 | `StudentsController::riskAnalysis` free of raw SQL | ✅ | Now calls `StudentService::verifyExists` first (`StudentsController.cc:170-185`) |
| 2.5 | Refactor preserved behavior | ✅ | Live: nonexistent student → **404** `Student not found`; missing `course_ids` → **400`**; happy path → **200** with risk payload. Delete: nonexistent → **404** `Enrollment not found`; own planned → **200** |
| 2.6 | All 6 controllers exercised (happy + error) | ✅ | See §4 for the full curl matrix |
| 3.1 | ≥5 tables with clear FKs | ✅ | `schema.sql`: **7** `CREATE TABLE`, **7** `REFERENCES` |
| 3.2 | README Mermaid ERD matches schema | ✅ | Live `information_schema.columns` dump matches all 7 entities **and every column, in order** |
| 3.3 | `current_gpa` clean on all four surfaces | ✅ | `PATCH .../grade` → `86.68`; `DELETE /enrollments/{id}` → `86.5`; `GET .../profile` → `86.68` then `86.5`; `GET .../academic-summary` → `86.68`. No float noise anywhere |
| 3.4 | Float-precision blast radius | ✅ | See §3 — tested with a deliberately non-binary-exact value |
| 3.5 | `localhost:5433` note matches reality | ✅ | Host `psql` → `FATAL: password authentication failed` (exit 2); documented fallback `docker compose exec db psql` → works (7 tables). README documents this as a **caveat**, which is accurate |
| 4.1 | Zero raw SQL / business logic in controllers | ✅ | `grep -nE "SELECT\|INSERT\|UPDATE\|DELETE FROM\|FROM \|WHERE \|execSql" controllers/*` → **exit 1 (no matches)** |
| 4.2 | README Route→Service paragraph accurate | ✅ | No `repositories/`, `query/`, or `repository/` directory exists; 26 `execSql*` calls live in the 4 services (Course 3, Enrollment 8, Student 9, User 6) |
| 4.3 | README hand-written-SQL paragraph accurate | ✅ | `models/` contains **only** `model.json`; repo-wide grep for `drogon_model`/`DROGON_MODEL` → no matches |
| 5.1 | Frontend build | ✅ | `npm run build` → `✓ built in 230ms`, 49 modules, exit 0 |
| 5.2 | Frontend lint | ✅ | `oxlint` exit 0; only the 2 known pre-existing `react(only-export-components)` warnings |
| 5.3 | No symlinks / submodules | ✅ | `find . -type l` → none; no `.gitmodules`; `git submodule status` → empty |
| 6.1 | Remote correct | ✅ | `origin https://github.com/nursala/smart_university_advisor.git` (fetch + push) |
| 6.2 | Nothing unpushed | ✅ | `0  0` |
| 6.3 | Fair commit distribution | ✅ | nursala 15 (+1 alt identity = 16), shadi 12, amerab 9 — 37 total, all three members substantial |
| 7.1 | README factually accurate end-to-end | ✅ | Cross-checked in §5 |
| 7.2 | `docs/auth-tests.md` count matches live run | ✅ | Doc says **34 cases / 199 assertions / 0 failures**; live run produced exactly that |

---

## 2. The four prior fixes — do they hold up?

### Fix 1 — `nour/layering-fix-controllers` ✅ HOLDS

Verified independently, not taken on trust:

- `grep` for SQL keywords and `execSql` across **all** controller `.cc`/`.h` files
  returns **exit 1 — no matches**. The only DB-related token in controllers is
  `drogon::app().getDbClient()`, which acquires a handle and passes it into a service.
  That is the Route-layer responsibility, not query logic.
- `EnrollmentsController::remove` (`EnrollmentsController.cc:149-190`) now performs
  lookup via `EnrollmentService::findForRemoval` (declared `EnrollmentService.h:37`,
  defined `EnrollmentService.cc:391`), then applies `AuthorizationService` checks, then
  calls `EnrollmentService::remove`.
- `StudentsController::riskAnalysis` (`StudentsController.cc:170-185`) now calls
  `StudentService::verifyExists` (declared `StudentService.h:53`, defined
  `StudentService.cc:556`) before parsing the body.
- **Behavior preserved**, confirmed live: `404 Student not found`,
  `400 course_ids is required`, `404 Enrollment not found`, `200` happy paths — the
  same status codes and error strings the pre-refactor endpoints produced.

### Fix 2 — `nour/gpa-rounding-gemini-refresh` ✅ HOLDS (and is stronger than described)

The fix is **belt-and-braces**, which is better than the summary suggested:

1. Every one of the four `current_gpa` **JSON output** sites applies explicit
   `std::round(x * 100.0) / 100.0`:
   `StudentService.cc:111-117` (profile), `StudentService.cc:155-160`
   (academic-summary), `EnrollmentService.cc:294-295` (recordGrade),
   `EnrollmentService.cc:372-377` (remove).
2. `config.json` additionally sets `float_precision_in_json: {precision: 15,
   precision_type: significant}` app-wide.

So `current_gpa` would still print cleanly even if the global config were reverted.
`StudentService.cc:313-317` reads `current_gpa` **without** rounding, but that value
feeds recommendation scoring internally and is never serialized — correct as written.

Live confirmation on all four required surfaces (§3.3 above): `86.68` and `86.5`,
never `86.68000000000001`.

The refreshed live-Gemini transcript in `docs/agent-demo.md` is present, dated
2026-07-28, labeled **"Evidence type: real Gemini"**, and its every factual detail
(GPA 86.5, 4 completed courses CS101/102/201/202, CS302 active in 2026-Spring, 16
completed credits) matches what the live API returns today. The claimed six-round cap
is real: `AgentLoop.h:30 → kDefaultMaxToolRounds = 6`.

### Fix 3 — `shadi/stale-docs-cleanup` ✅ HOLDS

- `docs/auth-tests.md` states **34 test cases / 199 assertions / 0 failures**. Live run
  this session: `All tests passed (199 assertions in 34 tests cases).` — exact match.
- `services/PasswordHasher.h` no longer carries the stale "decorative bcrypt, will NOT
  verify" warning; it now describes real `pbkdf2_sha256` seed hashes. **Verified true
  behaviorally**: both demo accounts logged in with **200** from a genuinely empty
  volume, twice.
- The README "Layer organization" rubric row now reads `Controller -> Service ->
  PostgreSQL ... there is no separate Repository layer`, which matches the code
  (§4.2 above).
- **The stale-volume bug that this task surfaced does not recur.** This is the single
  most important regression check, and it was exercised properly: `down --volumes`
  twice, with `seed.sql` genuinely re-run each time, and demo logins succeeding after
  each. A mid-audit deliberate mutation (GPA driven to `86.68`, enrollment 5 deleted, a
  temporary account registered) was fully erased by the reseed — post-reseed state
  returned to GPA `86.5`, 4 completed, 1 active, and the temp account's login correctly
  failed. That is positive proof the volume really is being recreated from `seed.sql`.

### Fix 4 — `shadi/document-architecture-decisions` ✅ HOLDS

- The README "Architecture decisions" section exists (`README.md:115-136`) with both
  paragraphs, and **both are accurate to the code as it exists right now**, verified
  independently (§4.2, §4.3).
- The `:5433` item took the **caveat** option (option B), and the caveat is **correct on
  this machine**: host-side `psql -h 127.0.0.1 -p 5433 -U advisor` fails with
  `FATAL: password authentication failed for user "advisor"`, exactly the
  Docker-Desktop/WSL2-dependent behavior the README warns about; the documented
  `docker compose exec db psql` fallback works. Because the README describes this as
  environment-dependent rather than asserting either outcome, it is accurate regardless
  of which machine the grader uses.

---

## 3. Float-precision blast radius — detailed

The global `float_precision_in_json` change affects **every** float in every response,
so this was tested against the fields that actually depend on it.

Exactly three float columns reach JSON (`grep "as<double>()" services/*.cc`):

| Field | Protected by explicit rounding? | Live observed value |
|---|---|---|
| `students.current_gpa` | **Yes** — `std::round(x*100)/100` at all 4 output sites | `86.5`, `86.68` — clean |
| `grades.grade` | No — relies solely on the app-wide config | `87.4`, `91.0`, `88.0`, `85.0`, `82.0` — clean |
| `course_prerequisites.minimum_grade` | No — relies solely on the app-wide config | `70` — clean |

The decisive test: `PATCH /enrollments/5/grade` with **`87.4`**, a value that is *not*
exactly representable in binary floating point (`87.4` → `87.40000000000000568...`).
The response returned:

```json
{"current_gpa":86.68,"enrollment_id":5,"grade":87.4,"passed":true,"status":"completed"}
```

`grade` printed as `87.4`, **not** `87.400000000000006`, and not truncated to `87`.
That is the strongest available evidence that the app-wide setting neither introduces
noise nor damages precision on an unrounded field. Raw (unparsed) `academic-summary`
output likewise shows `"grade":91.0` — correct float rendering, no noise.

### ⚠️ One requested check could not be performed as specified

The plan asked to verify `instructors.rating` specifically. **`rating` is not returned
by any endpoint.** `CourseService.cc` never selects it — `/courses` returns
`instructor_name` only, and `/courses/{id}/details` returns `instructor_id` +
`instructor_name`. Confirmed by reading both queries (`CourseService.cc:35-50`,
`:99-139`) and by inspecting live responses.

The column does hold genuinely noise-prone values (`4.5, 4.2, 4.7, 4.1, 4.6` — four of
which are not binary-exact), so it *would* have been the ideal probe, but it is
unreachable through the API. `grade` at `87.4` was used as an equivalent-strength
substitute. `rating` is therefore a **dead schema field**: present in `schema.sql`,
seeded, and drawn in the README ERD, but never surfaced. This is a cosmetic
observation, not a defect — the ERD documents the schema, and the schema does contain it.

---

## 4. Endpoint curl matrix (all 6 controllers, happy + error)

| Controller | Case | Result |
|---|---|---|
| Auth | `POST /auth/register` new | **201** |
| Auth | `POST /auth/register` duplicate | **400** `An account with this email already exists` |
| Auth | `POST /auth/login` wrong password | **400** `Invalid email or password` |
| Auth | `POST /auth/login` valid ×2 | **200** + token |
| Users | `GET /users/me` | **200** |
| Users | `GET /users/me` tampered token | **401** `Invalid or expired token` |
| Users | `PATCH /users/me` empty body | **400** `At least one of name or email must be provided` |
| Courses | `GET /courses` | **200**, 15 courses |
| Courses | `GET /courses/99999/details` | **404** `Course not found` |
| Courses | `GET /courses/5/details` | **200** + prerequisites |
| Students | `GET /students/1/profile` | **200** |
| Students | `GET /students/9999/profile` (admin) | **404** `Student not found` |
| Students | `GET /students/2/profile` (student) | **403** `Students may access only their own student record` |
| Students | `GET /students/1/available-courses` | **200** |
| Students | `POST /students/1/risk-analysis` | **200** `risk_level: Medium` |
| Students | `POST /students/1/risk-analysis` no body | **400** `course_ids is required` |
| Students | `POST /students/9999/risk-analysis` | **404** `Student not found` |
| Enrollments | `GET /enrollments/planned` | **200** |
| Enrollments | `POST /enrollments` valid | **201** |
| Enrollments | `POST /enrollments` duplicate | **409** `Course already has an active or planned enrollment` |
| Enrollments | `POST /enrollments` bad semester | **400** format message |
| Enrollments | `POST /enrollments` prereq unmet | **400** `Missing prerequisites: CS301` |
| Enrollments | `PATCH /enrollments/1/grade` as student | **403** `Students may not record official grades` |
| Enrollments | `PATCH /enrollments/5/grade` as admin | **200** |
| Enrollments | `DELETE /enrollments/99999` | **404** `Enrollment not found` |
| Enrollments | `DELETE /enrollments/45` own planned | **200** |
| Agent | `POST /agent/query` live | **200**, 3 tools chained |
| Agent | 6 further edge cases | 400 ×4, 401, 403 — see §1.5-1.6 |

Academic rules (prerequisites, repeat prevention, semester format, credit limit,
student-cannot-grade) are all enforced server-side, matching the README.

---

## 5. README cross-check

Every factual claim re-derived independently:

| README claim | Verified against | Result |
|---|---|---|
| "17 meaningful Drogon routes" + table | 17 `ADD_METHOD_TO`, paths extracted and diffed | ✅ exact |
| "7 tables and 7 foreign-key constraints" | `schema.sql` counts | ✅ 7 / 7 |
| Mermaid ERD entities + columns | live `information_schema.columns` | ✅ all 7 tables, all columns, correct order |
| `year_level` 1–6 | `schema.sql:48` `CHECK (year_level BETWEEN 1 AND 6)` | ✅ |
| `current_gpa` 0–100 | `schema.sql:51` | ✅ |
| `difficulty_level` in easy/medium/hard | `schema.sql:80` | ✅ |
| `enrollments.status` in planned/active/completed/dropped | `schema.sql:128` | ✅ |
| `UNIQUE (student_id, course_id, semester)` | `schema.sql:135` | ✅ |
| `UNIQUE (enrollment_id)` on grades | `schema.sql:140` | ✅ |
| "passing grade is 60" | `AcademicRules.h:8` `kPassingGrade = 60.0` | ✅ |
| "8 function tools" + named list | `ToolRegistry.cc:16-23` | ✅ names and order match |
| Six-step agent cap | `AgentLoop.h:30` `= 6` | ✅ |
| `Controller -> Service -> PostgreSQL`, no Repository layer | directory listing + 26 `execSql*` in services | ✅ |
| `models/model.json` is scaffold only, nothing generated | `ls models/`, repo-wide `drogon_model` grep | ✅ |
| `:5433` caveat + `docker compose exec` fallback | live `psql` attempt (fails) + fallback (works) | ✅ accurate |
| "SSE ... not implemented" (Known limitations) | grep for `text/event-stream`/`EventSource`/`newStreamResponse` | ✅ absent, claim honest |
| Demo credentials table | live logins ×2 from fresh volumes | ✅ both 200 |
| `docker compose up` single command | run twice from empty volume | ✅ |

`docs/auth-tests.md` — count matches the live run exactly (34 / 199 / 0). Its claim
that `/users/me` returns 401 for missing/malformed auth is confirmed. Note it does not
claim a 401 for wrong-password login, so there is no doc/behavior mismatch there.

No secrets are committed: only `.env.example` and `frontend/.env.example` are tracked.

---

## 6. Assignment requirement coverage

| Requirement | Weight | Status |
|---|---|---|
| Agentic loop + ≥8 function tools | 45% | ✅ 8 distinct read-only tools, real multi-step loop, 6-round cap, live Gemini 3-tool chain demonstrated |
| ≥10 API endpoints | 25% | ✅ 17 distinct meaningful routes |
| Database structure (PostgreSQL, ≥5 tables, FKs, ERD) | 20% | ✅ 7 tables, 7 FKs, accurate ERD in README |
| Readable code / correct layering | 10% | ✅ Clean Controller→Service split, zero SQL in controllers, deviation from taught 4-layer pattern documented and justified |
| Bonus: working UI | 10% | ✅ React + TypeScript, builds clean, lints clean |
| §3 ≥3-tool chained scenario | — | ✅ demonstrated live this session (3 tools) and in the recorded transcript (4 tools) |
| §4 auth (register / hashed pw / JWT / profile update) | — | ✅ all four present, PBKDF2-HMAC-SHA256 + HS256 JWT |
| §5 single `docker compose up` | — | ✅ verified twice from empty volume |
| §7 fair commit distribution | — | ✅ 16 / 12 / 9 across three members |

SSE and Zep are explicitly *recommendations*, not requirements; their absence is
disclosed honestly in the README's Known limitations.

---

## 7. What I could not verify in this environment

- **`instructors.rating` precision through the API** — the field is not exposed by any
  endpoint (§3). Substituted an equivalent test on `grade` with a non-binary-exact value.
- **Cross-machine `:5433` behavior** — I can only report this machine, where host auth
  fails and the README's caveat therefore reads correctly. The README's wording is
  deliberately environment-dependent, so it holds either way.
- **Gemini quota/latency under grading conditions** — one live call succeeded; sustained
  behavior under a grader's own key is outside this sandbox.

---

## 8. Deliberate mutations made during this audit (all reverted)

For transparency, Phase 3 required exercising mutating endpoints against the live DB:

1. `PATCH /enrollments/5/grade` → grade `87.4`, GPA `86.5` → `86.68`
2. `DELETE /enrollments/5` → GPA back to `86.5`
3. `POST /auth/register` → temporary account `audit.tmp@example.com`
4. `POST /enrollments` → enrollment id 45, then `DELETE`d

All were erased by a final `docker compose down --volumes && docker compose up --build`.
Post-reseed state verified pristine: GPA `86.5`, 4 completed / 1 active,
`audit.tmp@example.com` login correctly rejected, both demo logins **200**, full suite
**34 / 199 / 0**. **The demo database is currently in a clean, freshly seeded state.**

---

## 9. Observations (non-blocking, no follow-up task required)

Ordered by rubric weight × effort. **None of these block submission.**

1. **Fourth git identity inflates the contributor count** *(0% rubric weight, ~2 min
   effort, optional)*
   `git shortlog -sn --all` shows four names: `nursala` 15, `shadi` 12, `amerab` 9,
   and `nur salah` 1. The fourth is the same person as `nursala` — its email is
   `133213344+nursala@users.noreply.github.com`, a GitHub web-UI commit. A grader
   assessing §7 "fair distribution" reads the shortlog and may briefly see four
   contributors for a three-person group. Harmless and self-evident from the email, but
   a one-line `.mailmap` would collapse it. **Recommendation: leave it** — editing git
   identity this close to submission carries more risk than the cosmetic gain.

2. **`instructors.rating` is a dead schema field** *(0% weight, would require a code
   change)*
   Present in `schema.sql`, seeded with real values, drawn in the README ERD, but never
   returned by any endpoint. Not a defect — the ERD documents the schema faithfully, and
   an unused column is normal. Flagged only because the audit plan asked about it.
   **Recommendation: leave it.** Adding it to a response now would be an unnecessary,
   untested change to a passing build.

3. **Empty untracked directory `scratch_json_test.cc;C/`** *(0% weight, ~5 s)*
   A stray local artifact with a malformed name (likely a shell redirect accident). It
   is **empty and untracked** — `git status --porcelain --untracked-files=all` reports
   nothing, so it will not appear in a grader's clone. **Recommendation: leave it**;
   optionally `rmdir` it locally before a live screen-share demo.

4. **Minor REST nit: wrong-password login returns 400, not 401** *(no doc mismatch)*
   `POST /auth/login` with a bad password returns **400** `Invalid email or password`.
   401 would be the more conventional choice, but `docs/auth-tests.md` only claims that
   wrong-password and unknown-email return *the same* error (true, and good practice —
   it avoids user enumeration), and reserves its 401 claim for `/users/me`, which does
   return 401. No documentation is inaccurate. **Recommendation: leave it.**

---

## 10. Final verdict

**This repository is ready to submit as-is. No follow-up task is required.**

All four prior fixes were re-verified independently and all four hold. The
highest-risk item — the stale-volume regression that silently broke demo logins once
before — was tested the right way: two genuine `down --volumes` cycles, with a
deliberate mid-audit mutation proving the volume truly is recreated from `seed.sql`,
and demo logins returning 200 after each. The float-precision change was checked for
collateral damage using a value chosen specifically to expose it, and no other numeric
field was distorted.

Every factual claim in `README.md`, `docs/auth-tests.md`, and `docs/agent-demo.md` that
I could re-derive, I did re-derive, and all of them matched. The four observations in §9
are cosmetic, carry zero rubric weight, and each is safer left alone than changed on the
eve of submission.

The demo database is currently freshly seeded and the stack is running. The repo is
clean, pushed, and `0  0` against `origin/main`.
