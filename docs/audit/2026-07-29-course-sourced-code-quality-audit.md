# Course-sourced code quality audit — 2026-07-29

Read-only audit. Nothing was fixed; no file other than this report was created or
modified.

**Scope actually read, file by file (not sampled):** all 12 files in
`controllers/`, all 24 files in `services/`, both files in `filters/`, `main.cc`,
`database/schema.sql`, `test/test_main.cc` + `test/CMakeLists.txt`,
`models/model.json`, `CMakeLists.txt`, `Dockerfile`, `docker-compose.yml`,
`config.json`, and `README.md` top to bottom. Supporting reads:
`frontend/src/App.tsx`, `frontend/src/auth/AuthContext.tsx`,
`frontend/src/chat/ChatContext.tsx`, `docs/agent-demo.md`, `docs/auth-tests.md`,
`.env.example`.

**Live verification performed:** `docker compose down --volumes` followed by
`docker compose up --build` (full clean rebuild from an empty volume), then all
17 endpoints exercised over HTTP, the schema compared column-by-column against
the running PostgreSQL instance, the test binary run inside the API container,
and the build tree inspected inside the container.

Every citation below names the course source and slide it is checked against.

---

## Part A — Drogon-specific course alignment

### A.1 Layered architecture

**Checked against:** `LayersOfDragon.pdf` (Route → Service → Repository →
optional Query) and `Modern_Drogon_Web_Services.pdf` slide 12 (the worked
`GET /users` / `POST /users/add` example: Controller → `userService.getUsersList`
→ `userRepository.getAllUsers` → `client->execSqlAsync`).

**What the course teaches:** the repository is a distinct object. Slide 12 names
it explicitly — the service calls `userRepository.saveUser`, and only the
repository calls `execSqlAsync`. The service never sees the DB client.

**What this project does:** Controller → Service → `drogon::orm::DbClientPtr`.
There is no repository class anywhere; the service layer holds the SQL. The
README documents this as a deliberate simplification (README lines 117–126).

#### (a) "Zero raw SQL in any controller" — CONFIRMED, still accurate

Verified two ways. Every one of the six controller `.cc` files was read in full,
and a grep for `execSqlAsync|execSqlSync|newTransactionAsync|DbClientPtr|drogon::orm`
returns **zero hits in `controllers/`**. Every controller obtains the client with
`drogon::app().getDbClient()` purely to hand it to a service call:

| Controller | Service(s) it delegates to | Raw SQL |
| --- | --- | --- |
| `AuthController.cc` | `UserService`, `JwtService` | none |
| `UsersController.cc` | `UserService` | none |
| `CoursesController.cc` | `CourseService` | none |
| `StudentsController.cc` | `StudentService`, `AuthorizationService` | none |
| `EnrollmentsController.cc` | `EnrollmentService`, `AuthorizationService` | none |
| `AgentController.cc` | `AgentLoop`, `ToolRegistry`, `GeminiClient` | none |

The controllers do exactly what the Route Layer is supposed to do per
`LayersOfDragon.pdf`: validate input, authorize, call a service, map the result
to a response. Input validation is consistently delegated to
`ValidationHelpers` (`ValidationHelpers.h:12`), and result-to-HTTP mapping is a
single shared function (`ServiceResultHttp.h:7`) rather than per-controller
switch statements.

**One exception outside the controllers, worth naming for the defense.**
`filters/JwtAuthFilter.cc:69–93` issues its own query directly:

```cpp
drogon::app().getDbClient()->execSqlAsync(
    "SELECT id FROM students WHERE user_id = $1", ...);
```

This is the only place outside `services/` (and `main.cc`'s connection setup)
that touches the database. The README's phrasing "Controller -> Service ->
PostgreSQL" is literally true — a filter is not a controller — but it
under-describes the real topology. This is precisely the kind of drift a formal
Repository layer would have prevented, because the filter would have had to call
`studentRepository.findByUserId(...)` like everyone else. Severity:
style/consistency, not a defect. Good oral-defense material: *"we have one
route-layer component that bypasses the service layer, and here is why a
Repository would have caught it."*

#### (b) Consistent internal discipline in place of a Repository — largely YES

The question is whether, absent a Repository class, the services at least apply
one uniform DB idiom. Read across all four services, they do, on four axes:

1. **Error handling is 100% uniform.** Every single `execSqlAsync` call site in
   the project passes an error lambda with the identical shape:

   ```cpp
   [callback](const drogon::orm::DrogonDbException &exception) {
       LOG_ERROR << "<internal context>: " << exception.base().what();
       callback(ServiceResult::error("<user-safe message>"));
   }
   ```

   Verified at all 22 call sites: `StudentService.cc` (7), `EnrollmentService.cc`
   (7), `UserService.cc` (5), `CourseService.cc` (2), `JwtAuthFilter.cc` (1). The
   internal exception text is always logged and never leaked to the client. That
   is exactly the responsibility a Repository would centralize, applied by
   convention instead.

2. **One result type, one mapping point.** Every service returns `ServiceResult`
   (`ServiceResult.h:8`) with six statuses. It is translated to HTTP in exactly
   one place (`ServiceResultHttp.h:7`) and to agent-tool JSON in exactly one
   place (`ToolResult.h:9`). No service ever constructs an `HttpResponse`. This
   is strong layering discipline — arguably stronger than many Repository
   implementations achieve.

3. **Transaction style is uniform.** `EnrollmentService` is the only service that
   uses transactions, and all three mutating paths use an identical three-part
   idiom: `newTransactionAsync` → `SET TRANSACTION ISOLATION LEVEL SERIALIZABLE`
   → a `<op>InTransaction` free function in an anonymous namespace, with
   `transaction->rollback()` on the isolation-set failure path
   (`create` 215–243, `recordGrade` 319–342, `remove` 423–446). Three
   copies of the same shape, no drift.

4. **Shared queries are factored, not duplicated.** The eligibility query lives
   once in `StudentService::fetchAvailableCourses` (`StudentService.cc:52`) and is
   reused by `getAvailableCourses`, `getCourseRecommendations`, and
   `buildSemesterPlan`. The header comment (`StudentService.h:58–62`) explicitly
   says it is exposed so callers "don't re-implement the eligibility SQL." That
   is repository-layer thinking without the repository class.

**Two real idiom deviations found.** Both are the same shape: fetch the whole
table, then filter in C++ instead of in SQL.

- **`CourseService::searchCourses`** (`CourseService.cc:33–89`) issues
  `SELECT ... FROM courses c LEFT JOIN instructors i ... ORDER BY c.id` with **no
  `WHERE` clause at all**, then filters department / difficulty / credits /
  instructor in a C++ loop (lines 52–72). Every other filtered read in the
  codebase pushes its predicate into a parameterized `WHERE`.
- **`StudentService::analyzeRisk`** (`StudentService.cc:611–613`) issues
  `SELECT id, credits, difficulty_level, estimated_weekly_hours FROM courses`
  with no filter, loads all rows into a `std::map`, then looks up only the
  requested ids (lines 619–640). `WHERE id = ANY($1)` would do it in the database.

Worth saying out loud in the defense: **these are the safe way to be wrong.** By
never building a dynamic `WHERE`, the authors made SQL injection structurally
impossible in exactly the two endpoints where a naive implementation would have
concatenated user input into the query. The cost is that the catalog is fully
materialized per search. At seven tables and a small catalog that is immaterial;
at scale it would not be. Severity: style/consistency.

### A.2 Database-access approach

**Checked against:** `Dragon_Create_Database_model_and_classes.docx` — the course
teaches `drogon_ctl create model` to generate ORM classes per table as the
alternative to hand-written SQL.

**Chosen alternative confirmed applied consistently.**

- **100% parameterized.** 38 `$N` placeholders across 7 files. A grep for
  string-built SQL — concatenation into a query string, `sprintf`, `format(`, or
  `ostringstream << "...SELECT..."` — returns **zero matches** project-wide. Every
  user-controlled value reaches the database as a bound parameter.
- **No injection-shaped SQL exists anywhere.** This is not a "we found some but
  they're low risk" result; it is a clean zero. The only place a value is
  formatted into a string near SQL is `EnrollmentService.cc:174–177`, and that
  builds an *error message* (`"Credit limit exceeded: current=..."`) from values
  already read back out of the database, never a query.
- **No file uses a different data-access idiom.** Every DB call in the project is
  `execSqlAsync`; `EnrollmentService` additionally uses `newTransactionAsync`.
  Zero `execSqlSync`, zero coroutine (`co_await`) access, zero generated ORM model
  classes, zero use of Drogon's `Mapper<T>`.
- **The README's claim about `models/` is accurate.** `models/` contains only
  `model.json` (the generator config scaffold). No generated `.h`/`.cc` exists.
  `CMakeLists.txt:50` still wires the directory in via
  `aux_source_directory(models MODEL_SRC)`, but it compiles nothing — a harmless
  leftover from `drogon_ctl create project`.

---

## Part B — General C++ practice, against the course's own slides

Findings are bucketed by severity at the end of each subsection.

### B.1 Memory management — clean

**Checked against:** `04_Classes_Dynamic_Memory_Management.pdf`, slide 57
("C++ provides several mechanisms for dynamic memory management: through `new`
and `delete` expressions (discouraged); through the C functions `malloc` and
`free` (discouraged); through smart pointers and ownership semantics
(preferred)") and slide 61 ("Memory leaks can happen easily. Avoid explicit
memory management through `new` and `delete` whenever possible").

**Exact counts across all project code** (`controllers/`, `services/`,
`filters/`, `models/`, `test/`, `main.cc` — no vendored or Drogon-generated code
exists in the repo to exclude):

| Construct | Count | Locations |
| --- | --- | --- |
| `new` | **1** | `services/AgentLoop.cc:22` |
| `delete` / `delete[]` | **0** | — |
| `malloc` / `calloc` / `realloc` / `free` | **0** | — |

The single hit:

```cpp
// services/AgentLoop.cc:22
auto loop = std::shared_ptr<AgentLoop>(new AgentLoop(
    std::move(contents), ..., maxToolRounds));
```

**Category: (a) unavoidable framework/idiom interop.** `AgentLoop`'s constructor
is private (`AgentLoop.h:42`) to force construction through the `start()` factory,
and `std::make_shared` cannot invoke a private constructor. The raw pointer is
consumed by the `shared_ptr` constructor within the same expression, so ownership
becomes RAII-managed immediately and no `delete` is ever required or possible to
skip. This satisfies slide 61's *intent* ("avoid explicit memory management")
while technically using the `new` that slide 57 discourages.

For the defense, the standard way to remove even this one: add a private
`struct PassKey {};` tag, make the constructor public-but-unconstructable-outside,
and use `std::make_shared<AgentLoop>(PassKey{}, ...)`. That would take the project
to literally zero `new` and additionally collapse the two allocations
(control block + object) into one.

**Everything else is smart-pointer or RAII managed**, including:

- `std::make_unique<...Tool>()` ×8 (`ToolRegistry.cc:16–23`)
- `std::make_shared<GeminiClient>()` (`AgentController.cc:111`)
- `std::make_shared<Json::Value>()` ×2 (`AgentLoop.cc:276–277`)
- `std::enable_shared_from_this` to keep the loop alive across async hops
  (`AgentLoop.h:10`, used at `AgentLoop.cc:62, 98, 304`)
- Drogon's own `HttpRequestPtr` / `HttpResponsePtr` / `DbClientPtr` /
  `shared_ptr<Transaction>` throughout
- `std::vector` for every dynamically-sized buffer (`PasswordHasher.cc:19, 38`)

A textbook RAII example worth citing in the oral defense: `ScopedEnvVar`
(`test_main.cc:995–1025`) captures `JWT_SECRET` in its constructor and restores it
in its destructor, so `JwtServiceConstructorThrowsWithoutSecret` (line 1086)
cannot leak a cleared environment variable into any later test — even if the
`CHECK_THROWS_AS` itself throws.

**Verdict: no memory-safety risk. Leaks are impossible by construction.**

- Real issue: **none**
- Style/consistency: none
- Nitpick: the one `new` at `AgentLoop.cc:22` (mentioned once, here)

### B.2 Arrays — three findings, all style-level

**Checked against:** `03_References_Arrays_and_Pointers.pdf`, slide 57
("C-style arrays should be avoided whenever possible — use the `std::array`
type ... instead", with `std::vector` for the dynamically-sized case).

Grep for `type name[N]` declarations across all project code returns exactly five
hits. Three are string literals, which is not what slide 57 warns about:

- `services/Base64.cc:7` — `constexpr char kStandardTable[] = "ABC...";`
- `services/Base64.cc:9` — `constexpr char kUrlSafeTable[] = "ABC...";`
- `services/JwtService.cc:48` — `constexpr char kHeaderJson[] = R"({"alg":"HS256",...})";`

Two are genuine fixed-size buffers, both OpenSSL interop:

1. **`services/JwtService.cc:27`** — `unsigned char digest[EVP_MAX_MD_SIZE];`
   Output buffer for OpenSSL's `HMAC()`. **Category (a) framework interop**, but
   `std::array<unsigned char, EVP_MAX_MD_SIZE>` with `.data()` passed to `HMAC()`
   would satisfy slide 57 at zero runtime cost and identical ABI.
2. **`services/JwtService.cc:52`** — `unsigned char bytes[32];`
   Output buffer for `RAND_bytes()`. Same story. Note the code is careful: it uses
   `sizeof(bytes)` (line 53) rather than a repeated literal, and iterates with a
   range-`for` (line 56), so there is no array-decay length bug — only the type
   choice deviates from the slide.

One additional slide-57 case, the raw-pointer-as-array parameter:

3. **`services/Base64.h:13`** — `static std::string encode(const unsigned char *data, size_t length, ...)`.
   Slide 57's other named anti-pattern: a pointer and a separate length that the
   compiler cannot keep in sync. All three call sites pass them correctly
   (`PasswordHasher.cc:48–49` and `JwtService.cc:41–45`, each a matched
   `.data()`/`.size()` pair), so no bug exists today. The project compiles as
   **C++20** (`CMakeLists.txt:9–17`, confirmed by the `<coroutine>` probe), so
   `std::span<const unsigned char>` is available and would carry the length
   intrinsically.

**Everywhere else the guidance is followed.** `std::vector` is used for every
dynamically-sized sequence: `courseIds`, `uniqueCourseIds`, `availableCourses`,
`scores`, `order` (`StudentService.cc`), `salt`, `output`, `actualHash`,
`expectedHash` (`PasswordHasher.cc`), `decoded` (`Base64.cc`), `tools`
(`ToolRegistry.cc`). `PasswordHasher` is the direct counter-example to the two
`JwtService` buffers — it holds its salt and derived key in
`std::vector<unsigned char>` (lines 19, 38) rather than fixed C arrays, doing the
same OpenSSL interop the compliant way.

- Real issue: **none**
- Style/consistency: `JwtService.cc:27`, `JwtService.cc:52`, `Base64.h:13`
- Nitpick: none

### B.3 Project organization

**Checked against:** `09_Organizing_Larger_Projects.pdf`, slides 6, 11, 12.

#### Slide 11 — one `.h`/`.cc` pair per class, same directory

> "Generally, there should be one separate pair of header and implementation files
> for each C++ class. Very tightly coupled classes ... can be placed in the same
> header and implementation files."

**Compliant, with three exceptions I can name and defend.**

Every substantive class has its own co-located pair: `AgentLoop`, `GeminiClient`,
`JwtService`, `PasswordHasher`, `Base64`, `StudentService`, `CourseService`,
`EnrollmentService`, `UserService`, `AuthorizationService`, `AcademicRules`,
`ToolRegistry`, `JwtAuthFilter`, and all six controllers — 19 classes, 19 pairs,
each header and implementation in the same directory.

The exceptions, all falling under the slide's own "very tightly coupled" clause:

1. **`services/Tools.h` / `Tools.cc` holds 8 classes** (`GetStudentProfileTool`
   through `SearchCoursesTool`). Each is a two-method adapter over the same `Tool`
   interface (`Tool.h:11`), each is ~15 lines, and all eight are constructed
   together in one function (`ToolRegistry.cc:13–25`). Splitting them into 16
   files would be worse. This is the intended use of the exception clause.
2. **`CourseSearchFilters`** (`CourseService.h:12`) — a parameter object used by
   exactly one class, in that class's header.
3. **`AuthenticatedIdentity`** (`AuthorizationService.h:9`) — same pattern.

Five header-only files have no `.cc`: `ServiceResult.h`, `ServiceResultHttp.h`,
`ToolResult.h`, `JsonHelpers.h`, `Tool.h`. These are respectively a
struct-with-static-factories, two `inline` free functions, a function template,
and a pure abstract interface. A `.cc` would be empty. Not a violation.

#### Slide 12 — top-level namespace — **NOT followed; reporting the real finding**

> "Usually, there should be at least a top-level namespace (i.e. don't put stuff
> in the default namespace). Namespaces should group broadly similar or coherent
> functionality."

**Verified by grep across every `.cc` and `.h`, not assumed.** The result:

- **Exactly one named namespace exists in the entire codebase:**
  `namespace ValidationHelpers` (`ValidationHelpers.h:12`).
- **Every class is declared in the global namespace** — all 6 controllers, all 13
  service and filter classes, every struct. There is no `sua::`, `advisor::`, or
  equivalent top-level namespace anywhere.

So slide 12's guidance is **not met**. This is the shortcut the brief anticipated,
and it is worth having a straight answer ready: `drogon_ctl create controller`
scaffolds classes into the global namespace by default, and Drogon's
`ADD_METHOD_TO` / filter-name-string registration (`"JwtAuthFilter"` in
`AgentController.h:10` and elsewhere) resolves against unqualified names, so the
project kept the scaffold's convention. It is a genuine deviation, low-risk in a
single-binary project with no external consumers, and cheap to fix.

**What the project does do correctly on the same slide's subject:** it uses
**20 anonymous namespaces across 16 files** for file-local helpers — `main.cc:8`,
every controller `.cc`, every service `.cc`. This is not incidental: five of the
six controllers define a function with the identical signature
`drogon::HttpResponsePtr errorResponse(const std::string &, drogon::HttpStatusCode)`
(`AuthController.cc:11`, `CoursesController.cc:11`, `StudentsController.cc:14`,
`EnrollmentsController.cc:14`, `UsersController.cc:10`, plus
`AgentController.cc:33`). Without the anonymous namespaces those six definitions
would be an ODR violation and a link error. The internal-linkage discipline is
correct even though the top-level namespace is missing.

#### Slide 6, part 1 — tests in a separate directory tree — **CONFIRMED**

> "Tests should reside in a separate directory tree from the actual
> implementation."

Verified rather than assumed. `test/` is a separate top-level tree containing
`CMakeLists.txt`, `test_main.cc` (34 test cases), `mock_gemini_server.js`, and
`test/integration/` with three PowerShell integration scripts. There is **zero
test code in `controllers/` or `services/`** — no `*_test.cc`, no `#ifdef TESTING`
blocks, no test-only files mixed into the implementation directories.

One narrow caveat, not a slide-6 violation: `test/CMakeLists.txt` builds the
service sources by reaching back across the tree
(`${CMAKE_CURRENT_SOURCE_DIR}/../services/StudentService.cc`, etc.) rather than
linking a shared library target. The *tests* are properly separate; the *build
wiring* is coupled, and the 12 source paths are hand-listed across two calls
(lines 4–19), so adding a service means remembering to edit this file. Nitpick.

The two production-code concessions to testing are both narrow and documented:
`JwtService::issueWithoutBootClaimForTesting` (`JwtService.h:39`) and the
`GEMINI_API_HOST` override (`GeminiClient.h:17–19`). Both are named and commented
as test hooks rather than hidden.

#### Slide 6, part 2 — out-of-source build — **CONFIRMED LIVE**

> "Out-of-source builds should always be preferred."

`Dockerfile:7` does `cmake -S . -B build && cmake --build build --parallel`.
Verified inside the freshly built running container:

```
$ ls /app/build
CMakeCache.txt  CMakeFiles  cmake_install.cmake  Makefile
smart_university_advisor  test

$ find /app/controllers /app/services /app/filters \
       -name '*.o' -o -name 'CMakeCache.txt' -o -name 'Makefile'
(no output)
```

All artifacts, both binaries, and the CMake cache live in `build/`. The source
directories are completely clean of build output. Fully compliant. `.gitignore`
and `.dockerignore` exist to keep it that way.

- Real issue: **none**
- Style/consistency: no top-level namespace (slide 12) — the one substantive
  finding in B.3
- Nitpick: hand-listed sources in `test/CMakeLists.txt`; `AgentLoop.cc:11` and
  `StudentsController.cc:44` close an anonymous namespace without the
  `}  // namespace` comment every other file uses; `StudentsController.cc` opens
  two adjacent anonymous namespace blocks (12–23, 25–44) that could be one

### B.4 Concurrency — no manual threading exists; this is the good outcome

**Checked against:** `07_Concurrency_in_Modern_Hardware.pdf` and
`08_Parallel_Programming.pdf` ("Trying to acquire a lock on an already locked
mutex will block the thread until the mutex becomes [available] ... Requires
careful implementation to avoid deadlocks").

**Grep results across every `.cc` and `.h` in the project:**

| Construct | Count |
| --- | --- |
| `std::mutex` / `std::shared_mutex` | **0** |
| `std::lock_guard` / `std::unique_lock` | **0** |
| manual `.lock()` / `.unlock()` | **0** |
| `std::atomic` | **0** |
| `std::condition_variable` | **0** |
| `std::async` | **0** |
| `.detach()` | **0** |
| `std::thread` | **1** — `test/test_main.cc:1104` |

**Stating it plainly, as the brief asks: the project introduces no manual
threading and no manual locking anywhere in the application.** There is no
deadlock surface to audit, no lock-ordering question to answer, and no
manual `lock()`/`unlock()` pair that could be skipped on an early return or
exception path. That is a valid and good result, not a gap in this audit.

The single `std::thread` is in the test harness `main()`
(`test_main.cc:1096–1118`) and is the standard Drogon test pattern: run
`app().run()` on a second thread, synchronize startup with a
`std::promise`/`std::future` pair (lines 1100, 1106, 1111), then
`queueInLoop([]{ app().quit(); })` and `thr.join()` (lines 1115–1116). The
promise/future *is* the synchronization primitive; there is no shared mutable
state guarded by hand, and the thread is joined, never detached.

The application itself (`main.cc:42–63`, `config.json` `number_of_threads: 4`)
runs entirely on Drogon's single-reactor-per-thread model, with all handler work
in async callbacks. Two details show this was reasoned about rather than
inherited by accident:

- **`main.cc:27–39`** sizes the DB connection pool to match the IO thread count,
  with a comment stating why: *"one DB connection per IO thread avoids threads
  blocking on each other for a pooled connection under concurrent load."* That is
  a concurrency design decision made by sizing rather than by locking — the right
  instinct for this model.
- **Shared immutable state uses magic statics, not locks.** The three pieces of
  cross-request state that could have tempted a mutex all use function-local
  `static const` initialization, which C++11 guarantees is thread-safe and
  once-only, and are never mutated afterward:
  `ToolRegistry`'s `tools()` and `toolsByName()` (`ToolRegistry.cc:27–44`),
  `processBootId()` (`JwtService.cc:62–66`), and the compiled
  `static const std::regex` in `AcademicRules.cc:7`. This is the correct
  lock-free pattern.
- **Per-request mutable state is confined to one owner.** `AgentLoop`'s mutable
  members (`contents_`, `toolRounds_`, `finished_`) live inside a
  `shared_from_this`-kept object driven by a strictly sequential callback chain —
  `executeFunctionCalls` recurses one tool at a time (`AgentLoop.cc:281–321`),
  never fanning out — so no two threads ever touch one `AgentLoop`. The
  `finished_` flag is checked at the top of every entry point (lines 53, 83, 258,
  286, 324, 352) as a re-entrancy guard, which is the correct approach for a
  sequential async chain and does not need to be atomic.

Real database concurrency is handled where it belongs — in PostgreSQL, not in
C++: `SET TRANSACTION ISOLATION LEVEL SERIALIZABLE` plus `SELECT ... FOR UPDATE`
row locks in all three `EnrollmentService` mutations
(`EnrollmentService.cc:103, 225, 263, 326, 354, 430`).

- Real issue: **none**
- Style/consistency: none
- Nitpick: none

### B.5 Standard Library usage — **general practice, not course-sourced**

> **Labeled per the brief.** `06_Standard_Library_C.pdf` could not be read (the
> file is not a valid PDF — likely an incomplete upload), so **nothing in this
> subsection is grounded in a course source.** These are general C++ practice
> observations only and should not be presented in the oral defense as
> "the course taught X."

#### Hand-rolled loops an STL algorithm would express better

- **`StudentService.cc:379–383` and `476–480`** — the same four-line index-fill
  loop appears twice:
  ```cpp
  std::vector<size_t> order(availableCourses.size());
  for (size_t i = 0; i < order.size(); ++i) { order[i] = i; }
  ```
  `std::iota(order.begin(), order.end(), 0)` from `<numeric>` is the idiom.
- **`StudentService.cc:632–640`** — a loop that returns an error if any requested
  id is missing from the map is `std::any_of` / `std::all_of` spelled long-hand.
- **`JsonHelpers.h:10`'s `toJsonArray`** is a well-judged in-house algorithm whose
  own comment says it "replaces the repeated ... loop pattern used across the
  service layer" — but adoption is only partial. It is used at
  `StudentService.cc:263` and `CourseService.cc:152`, while the identical loop is
  still hand-written at `EnrollmentService.cc:44–63`,
  `StudentService.cc:172–223`, and `ToolRegistry.cc:49–54`. Finishing the
  migration would be a genuine consistency win.

**One hand-rolled loop that must NOT be "simplified":**
`PasswordHasher.cc:117–122` XORs the full hash byte-by-byte and accumulates into
`diff` rather than short-circuiting. That is deliberate and correct — the comment
at lines 115–116 explains it is constant-time so a failed match cannot leak how
many leading bytes were right. `std::equal`, `==`, or `memcmp` would all
short-circuit and reintroduce the timing side channel. Flagging it here so a
future reviewer running a "replace loops with STL" pass does not break it.
`JwtService.cc:68–72` handles the equivalent comparison the other correct way, via
OpenSSL's `CRYPTO_memcmp`.

The codebase is not STL-averse overall: `std::stable_sort` ×2,
`std::sort` + `std::unique` + `erase` (the correct dedup idiom,
`StudentService.cc:606–609`), `std::transform` ×2 (`AgentController.cc:20`,
`CourseService.cc:15`), `std::find`, `std::min`, `std::max` are all used
idiomatically.

#### Container choice

- **`ToolRegistry.cc:33`** — `std::map<std::string, const Tool *>` built once and
  used only for exact-name `.find()`. Ordering is never used;
  `std::unordered_map` is the better fit. With 8 entries this is immaterial at
  runtime — it is a signalling point, not a performance one. Nitpick.
- **`StudentService.cc:619`** — `std::map<int64_t, AvailableCourse> coursesById`,
  built then used only via `.find()` / `.at()`. Same observation. Nitpick.
- **Good choices worth crediting:** `std::optional` is used properly for genuinely
  absent values rather than sentinel numbers — nullable GPA
  (`StudentService.cc:313, 596`), optional search filters
  (`CourseService.h:14–17`), optional `studentId` on a staff identity
  (`AuthorizationService.h:13`). `std::vector` is used for every sequence.

#### Efficiency

- **`Base64.cc:64`** — `const std::string table = urlSafe ? kUrlSafeTable : kStandardTable;`
  constructs a heap-allocated `std::string` on **every** `decode()` call, purely so
  the code can call `.find()`, which is then an O(64) linear scan **per input
  character**. `decode()` runs on every JWT verification, i.e. on every
  authenticated request. A `constexpr std::array<int8_t, 256>` reverse-lookup
  table would be allocation-free and O(1) per character; even
  `std::string_view` would remove the allocation. Small but real, and on a hot
  path.
- The two fetch-all-then-filter-in-C++ sites from A.1(b)
  (`CourseService.cc:33`, `StudentService.cc:611`) are the other efficiency
  observation.

- Real issue: **none**
- Style/consistency: partial `toJsonArray` adoption; `Base64.cc:64` per-call
  table allocation; the two `iota` loops; the fetch-all filters
- Nitpick: `std::map` vs `std::unordered_map` at two sites

---

## Part C — Full line-by-line README audit

Every command below was executed verbatim against a **completely fresh stack**:
`docker compose down --volumes` (volume destroyed and recreated) followed by
`docker compose up --build` (full image rebuild). Endpoint checks ran against
that rebuilt stack with the database seeded from `database/seed.sql`.

### Run section (lines 6–34) — accurate

| Claim | Result |
| --- | --- |
| `docker compose up` starts the stack | Verified. `api`, `db` (healthy), `frontend` all up |
| `docker compose down --volumes && docker compose up --build` | Verified end to end; clean rebuild from an empty volume, no manual steps |
| API listens on `http://localhost:8080` | Verified |
| `curl http://localhost:8080/courses` | `200`, returns the seeded catalog |
| Frontend on `5173` | `200` (`docker-compose.yml:49` maps `5173:80`) |
| `DB_NAME` / `DB_USER` / `DB_PASSWORD` overridable, defaults as stated | Verified (`main.cc:51–53`, `docker-compose.yml:5–7, 30–32`) |
| `JWT_SECRET` falls back to an insecure dev default; see `.env.example` | Verified (`docker-compose.yml:34`); `.env.example` exists and documents it |

**The `:5433` host-auth caveat (lines 18–26) is not just accurate — it reproduced
on this machine.** Connecting through the forwarded port fails exactly as
described:

```
psql -h <host> -p 5433 -U advisor -d smart_university_advisor
psql: error: ... FATAL:  password authentication failed for user "advisor"
```

while the documented workaround works:

```
docker compose exec db psql -U advisor -d smart_university_advisor
 container-internal psql OK
```

The README's careful hedging ("some Docker Desktop/WSL2 configurations have been
observed to reject those same credentials") is the correct claim, empirically
confirmed. This is the kind of documented-caveat-that-actually-reproduces that is
worth pointing at in a defense.

### Schema / ERD (lines 36–113) — accurate, one label inconsistency

**Full column-by-column pass, not a spot-check.** The README ERD was compared
against `database/schema.sql` *and* against the live `information_schema` of the
running database (42 columns across 7 tables).

- **7 tables** — live count: `tables=7` ✓
- **7 foreign-key constraints** — live count: `fks=7` ✓
- **All 42 columns match** in name, type, and declared order, for all 7 entities:
  `USERS` (6), `INSTRUCTORS` (5), `STUDENTS` (7), `COURSES` (9),
  `COURSE_PREREQUISITES` (4), `ENROLLMENTS` (6), `GRADES` (5). No column is
  missing from the ERD and none is invented.
- **All 7 FK arrows are correct**, including both `COURSES → COURSE_PREREQUISITES`
  edges (`course_id` and `prerequisite_course_id` both reference `courses.id`,
  `schema.sql:89–97`), which the ERD correctly draws as two separate
  relationships with distinct labels.
- **Key constraints paragraph (lines 109–113)** — every one verified:
  `year_level` 1–6 (`schema.sql:47`), `current_gpa` 0–100 (line 50),
  `difficulty_level` enum (line 79), `enrollments.status` enum (line 127),
  `UNIQUE (student_id, course_id, semester)` (line 134), `UNIQUE (enrollment_id)`
  on `grades` (line 140).

**One finding (nitpick).** ERD line 68 marks `students.user_id` as `FK` only, but
the schema declares it `BIGINT NOT NULL UNIQUE` (`schema.sql:34`), confirmed in
the live unique-constraint listing. The ERD marks the structurally identical
`grades.enrollment_id` as `FK, UK` (line 102), so the labeling is internally
inconsistent. It should read `bigint user_id FK, UK`.

**Mermaid syntax.** No Mermaid renderer is installed locally and I did not fetch
one, so I validated the block by grammar inspection rather than by rendering, and
am flagging the method rather than overclaiming. The block is well-formed: a
correct `erDiagram` header, 7 relationship lines using valid cardinality
operators (`||--o|`, `||--o{`) with quoted labels, and 7 attribute blocks whose
entries follow `type name [keyType]`. The two `COURSES ||--o{ COURSE_PREREQUISITES`
lines carry distinct labels so they do not collide, and the comma-separated
`FK, UK` key form on line 102 is supported. Nothing in it is syntactically
suspect.

### Endpoints (lines 138–160) — all 17 verified live

"17 meaningful Drogon routes" is exact: counting `ADD_METHOD_TO` across the
controller headers gives Auth 2 + Users 2 + Courses 2 + Students 6 +
Enrollments 4 + Agent 1 = **17**. The table has exactly 17 data rows. Method,
path, and description were each checked against the controller code and then
exercised:

| # | Documented route | Code | Live result |
| --- | --- | --- | --- |
| 1 | `POST /auth/register` | `AuthController.h:9` | `201`, returns `{token, user}` with linked `student_id` |
| 2 | `POST /auth/login` | `AuthController.h:10` | `200`, `{token, user}` |
| 3 | `GET /users/me` | `UsersController.h:9` | `200` with Bearer; `401` without |
| 4 | `PATCH /users/me` | `UsersController.h:10` | `200`, name updated |
| 5 | `GET /courses` | `CoursesController.h:10` | `200`, public |
| 6 | `GET /courses/{id}/details` | `CoursesController.h:11` | `200`, includes description/credits/difficulty/prerequisites |
| 7 | `GET /students/{id}/profile` | `StudentsController.h:10` | `200`, all 6 documented fields present |
| 8 | `GET /students/{id}/academic-summary` | `StudentsController.h:13` | `200`, GPA + counts + credits |
| 9 | `GET /students/{id}/available-courses` | `StudentsController.h:16` | `200` |
| 10 | `POST /students/{id}/course-recommendations` | `StudentsController.h:19` | `200`, scored with reasons |
| 11 | `POST /students/{id}/semester-plan` | `StudentsController.h:22` | `200`, respected `max_credits: 12` |
| 12 | `POST /students/{id}/risk-analysis` | `StudentsController.h:25` | `200`, Low/Medium/High + reasons |
| 13 | `POST /enrollments` | `EnrollmentsController.h:12` | `201`, created as `planned` |
| 14 | `GET /enrollments/planned` | `EnrollmentsController.h:10` | `200` |
| 15 | `PATCH /enrollments/{id}/grade` | `EnrollmentsController.h:14` | `403` as student, `200` as admin, marks `completed` |
| 16 | `DELETE /enrollments/{id}` | `EnrollmentsController.h:17` | `200`, GPA recalculated |
| 17 | `POST /agent/query` | `AgentController.h:9` | `200` against **live Gemini** |

The agent call returned `{"answer":"Your current GPA is 86.5.", "tools_used":["get_student_profile"], "status":"ok"}` —
a real end-to-end tool call, not a mock.

Two small description gaps:

- **Line 157**, `GET /enrollments/planned` — "List **the authenticated student's**
  planned enrollments." Staff may also pass `?student_id=N` to list another
  student's (`EnrollmentsController.cc:37–63`). Right for the student case,
  incomplete for staff. Nitpick.
- **Line 154**, `POST /students/{id}/semester-plan` — "within a credit limit" is
  accurate but does not mention that the requested `max_credits` is clamped down
  to the student's configured `max_weekly_credits`
  (`StudentService.cc:464–465`), so a request for more than the student's limit
  silently yields the smaller plan. Nitpick.

### Agent tools (lines 238–256) — all 8 match, one file pointer is off

The 8 tool names and **their exact order** match `ToolRegistry.cc:16–23`
one-for-one: `get_student_profile`, `get_academic_summary`,
`get_available_courses`, `get_course_recommendations`, `build_semester_plan`,
`analyze_academic_risk`, `get_course_details`, `search_courses`. This is also
asserted in the test suite (`test_main.cc:261–282`), which additionally checks
that no `enroll_in_course` tool exists — matching the README's read-only claim.

**Finding (style/accuracy).** Lines 241–242 say the 8 tools are *"declared in*
`services/ToolRegistry.cc`*"*, and line 265 repeats *"8 read/analysis
declarations in `services/ToolRegistry.cc`."* The declarations — name,
description, and JSON-schema parameters — are actually written in
**`services/Tools.cc`** (each class's `declaration()` method, lines 98–436).
`ToolRegistry.cc` only *instantiates and orders* them. The count and the names
are correct; the file pointer is one hop off, so an examiner opening
`ToolRegistry.cc` to find the 8 declarations will instead find 8
`make_unique` lines.

### Rubric table (lines 258–273) — one more file pointer off

Twelve rows, all substantively true. `frontend/src/App.tsx` exists; the Docker
Compose, schema, and ERD rows are verified above.

**Finding (style/accuracy), line 267.** "Real agentic loop | Six-step-capped loop
in `controllers/AgentController.cc`." The loop and its cap live in
**`services/AgentLoop.{h,cc}`** — `kDefaultMaxToolRounds = 6` is `AgentLoop.h:30`,
and the round-counting plus final-synthesis logic is `AgentLoop.cc:51–115`.
`AgentController.cc:156–199` only calls `AgentLoop::start(...)` and does not even
pass a cap. Same one-hop-off issue as the tools row.

**Minor completeness, line 273.** "Layer organization | `Controller -> Service ->
PostgreSQL` (services call `drogon::orm::DbClientPtr` directly; there is no
separate Repository layer)" is accurate as far as it goes, but omits that
`filters/JwtAuthFilter.cc:69` also queries the database directly (see A.1(a)).
The Architecture-decisions section (lines 117–126) has the same omission, naming
only the four services. In an otherwise unusually honest section, this is the one
place the self-description is slightly rosier than the code.

### Auth section (lines 162–206) — accurate, verified live

| Claim | Verification |
| --- | --- |
| Profile / Courses / AI Advisor / My Plan pages | `App.tsx:32–35` (`/chat`, `/courses`, `/profile`, `/my-plan`) |
| Student login → Profile; staff login → catalog | `App.tsx:18` — `role === 'student' ? '/profile' : '/courses'` |
| `/dashboard` is only a compatibility redirect | `App.tsx:31` maps it to `RoleHome`, which renders only `<Navigate>` (lines 16–18) |
| Chat survives navigation, cleared by refresh/logout, React memory only | `ChatContext.tsx:25` — plain `useState`, no storage API anywhere in the file |
| PBKDF2-HMAC-SHA256, not bcrypt | `PasswordHasher.cc:20`, 210 000 iterations (`PasswordHasher.h:36`) |
| HS256 JWTs verified by `JwtAuthFilter` | `JwtService.cc:48, 135`; `JwtAuthFilter.cc:42` |
| Student/enrollment/agent routes require JWT; catalog public | Verified live: `/courses` → `200` unauthenticated; `/students/1/profile` → `401`; `/agent/query` → `401` |
| Auth candidates in `sessionStorage`, validated via `/users/me` | `AuthContext.tsx:35, 73, 105, 120`; `localStorage` is actively *cleared* (lines 33, 55, 104) |
| Random per-process `server_boot_id` invalidates old tokens | `JwtService.cc:50–66`; visible in the decoded live token payload |
| Registration atomically creates `users` + `students`, returns `{token, user}` | `UserService.cc:48–65` (single CTE); live `201` |
| New students: `Undeclared`, year 1, 20-credit limit, NULL GPA | **Verified live** — registered a probe account, queried the DB: `Undeclared \| 1 \| 20 \| gpa_is_null=t \| student`. Probe account deleted afterward. |
| Both seeded credentials work | Verified live — `adam@example.com`/`DemoStudent2026!` and `admin@example.com`/`DemoStaff2026!` both return `200` |
| Students cannot record grades; staff can | Verified live — student `403` "Students may not record official grades"; admin `200` |
| Agent tool arguments are server-scoped | `ToolRegistry.cc:105–113` overwrites `student_id`; asserted at `test_main.cc:251–259` |

### Academic and enrollment rules (lines 208–236) — accurate

Canonical semester format matches both the C++ regex (`AcademicRules.cc:8`) and
the database CHECK constraint (`schema.sql:132`) — the same pattern in both
places. Passing grade 60 matches `AcademicRules::kPassingGrade`
(`AcademicRules.h:8`) and `chk_grades_passed_consistency` (`schema.sql:154`). The
prerequisite rule ("completed enrollment with a passing grade that also meets the
configured minimum") matches the SQL at `StudentService.cc:73–78` and
`EnrollmentService.cc:110–114`. Retake, duplicate, and credit-limit rules match
the CTE at `EnrollmentService.cc:115–134`.

The GPA claim is precise and correct: `AVG` over completed graded enrollments
including failures (`EnrollmentService.cc:274–281`), `NULL` when none exist
(`schema.sql:39`), and updated atomically inside the same statement as the
mutation for both grade recording and deletion. Confirmed live: recording an 88
moved the GPA `86.5 → 86.8`, and deleting that enrollment returned it to `86.5`
in the same response.

### Known limitations (lines 275–285) — all 5 still true

1. Chat survives navigation but not refresh — true (`ChatContext.tsx:25`, memory only).
2. `sessionStorage`-based browser-tab auth — true (`AuthContext.tsx`).
3. No staff administration portal — true; `frontend/src/pages/` contains only
   Chat, Courses, Login, MyPlan, Profile, Register.
4. No SSE, no persistent conversational memory — true; `GeminiClient.cc:69` uses a
   single `sendRequest`, and no conversation is persisted anywhere.
5. Live Gemini needs a locally configured key; no secrets in the repo — true;
   `.env.example` holds a placeholder and `.env` is gitignored.

### Internal links and formatting — clean

All 10 markdown links resolve to existing files: `database/schema.sql` (×2),
`services/ToolRegistry.cc` (×2), `docs/agent-demo.md` (×3),
`controllers/AgentController.cc`, `docker-compose.yml`,
`frontend/src/App.tsx`. Inline file references (`services/PasswordHasher.cc`,
`services/JwtService.cc`, `filters/JwtAuthFilter`, `models/model.json`,
`.env.example`) all exist too. No link targets a section anchor, so there are no
broken anchors to check.

Formatting: 6 code-fence markers = 3 balanced blocks, none unclosed. Table
structure is consistent — 23 three-column rows (endpoints: header + separator +
17 data = 19; credentials: header + separator + 2 data = 4) and 14 two-column
rows (rubric: header + separator + 12 data). No ragged or broken rows.

One note for completeness: the brief mentioned `docs/auth-tests.md` as an example
internal link, but the README does not actually link it — only `docs/agent-demo.md`.
The file exists, and its central claim checks out: running the suite inside the
container gave `All tests passed (199 assertions in 34 tests cases)`, matching the
documented "34 test cases, 199 assertions, 0 failures."

---

## Consolidated severity summary (Part B and A)

**Real issue (memory-safety risk, injection-shaped SQL, deadlock-risk locking):**
**none found.** Zero `delete`/`malloc`/`free`, zero string-built SQL, zero
mutexes. This is a genuine clean result on all three axes the brief singled out,
not a soft pass.

**Style / consistency:**

| Finding | Location | Source |
| --- | --- | --- |
| No top-level namespace; every class is global | project-wide (only `ValidationHelpers` is named) | 09, slide 12 |
| Filter queries the DB directly, bypassing the service layer | `filters/JwtAuthFilter.cc:69` | LayersOfDragon / Modern Drogon slide 12 |
| Fetch-all-then-filter-in-C++ breaks the parameterized-`WHERE` idiom | `CourseService.cc:33`, `StudentService.cc:611` | LayersOfDragon (A.1b) |
| Two OpenSSL C-array buffers | `JwtService.cc:27, 52` | 03, slide 57 |
| Raw pointer-as-array parameter | `Base64.h:13` | 03, slide 57 |
| `toJsonArray` adopted at 2 of ~5 eligible sites | `EnrollmentService.cc:44`, `StudentService.cc:172`, `ToolRegistry.cc:49` | general practice |
| Per-call `std::string` table allocation in a hot decode path | `Base64.cc:64` | general practice |
| Duplicated index-fill loops (`std::iota`) | `StudentService.cc:379, 476` | general practice |
| Tool declarations attributed to the wrong file | README 241, 265 | Part C |
| Agent loop / six-round cap attributed to the wrong file | README 267 | Part C |

**Nitpick (mentioned once each, not padded):** the single `new`
(`AgentLoop.cc:22`); `std::map` where `unordered_map` fits
(`ToolRegistry.cc:33`, `StudentService.cc:619`); two missing `}  // namespace`
comments (`AgentLoop.cc:11`, `StudentsController.cc:44`); two adjacent anonymous
namespaces in `StudentsController.cc`; hand-listed sources in
`test/CMakeLists.txt`; `JwtAuthFilter.h`'s comment omits the `student_id`
attribute it also sets; `students.user_id` missing its `UK` marker in the ERD;
`GEMINI_MODEL`'s default lives in `docker-compose.yml:36`, not in code
(`GeminiClient.cc:33` throws on empty) — true for the documented Docker path, but
worth knowing; two incomplete endpoint descriptions (README 154, 157).

---

## Honest closing assessment

**Does the codebase reflect what this course taught?** On the C++ fundamentals,
yes — and more convincingly than a checklist pass would suggest, because the
compliance is structural rather than cosmetic. One `new` and zero `delete` in the
entire project is not an accident of a small codebase; it is the result of
`make_unique`/`make_shared`/`enable_shared_from_this` being the reflexive choice
everywhere, with `std::vector` and `std::optional` doing the work that raw buffers
and sentinel values would do in weaker code. The complete absence of manual
locking is likewise a design position, not an omission: the two pieces of shared
state that would have tempted a mutex use magic statics instead, and the DB pool
is deliberately sized to the reactor's thread count with a comment explaining
why. The Drogon layering material is followed in the part that matters most —
zero SQL in any controller, one uniform error-handling shape across all 22 query
sites, one `ServiceResult` type with a single mapping point to HTTP and a single
one to tool JSON — which is the discipline a Repository layer exists to enforce,
achieved by convention instead of by class. Two genuine deviations are worth
owning rather than glossing: **no top-level namespace** (slide 12 is explicit, and
the project is entirely in the global namespace — a Drogon-scaffold habit, cheap
to fix, low-risk here only because 20 anonymous namespaces absorb the helper-name
collisions that would otherwise be link errors), and **one route-layer filter that
queries the database directly**, which is the concrete cost of having no
Repository class and a much better answer to "why does the course want a
Repository?" than any abstract argument. Both are style-level, both are
defensible, and neither is a defect. The deliberate departures the README already
documents — hand-written SQL over `drogon_ctl create model`, Service-absorbs-
Repository — are consistently executed rather than half-applied, which is the
thing that actually distinguishes a considered simplification from a shortcut.

**Is the README fully accurate line by line?** Very nearly, and everything
load-bearing checks out under live verification: all 17 endpoints behave exactly
as documented against a stack rebuilt from an empty volume, all 8 tool names match
in exact order, all 42 ERD columns match both `schema.sql` and the running
database, all 5 known limitations are still true, all 10 links resolve, the
formatting is clean, and the `:5433` host-auth caveat actually reproduced on this
machine rather than being defensive boilerplate. The remaining gap is specific and
narrow: **three file attributions point one hop away from the code they
describe.** The 8 tool declarations are in `services/Tools.cc`, not
`ToolRegistry.cc` (README 241, 265); the six-round agentic loop and its cap are in
`services/AgentLoop.{h,cc}`, not `AgentController.cc` (README 267); and the
architecture description omits that `filters/JwtAuthFilter.cc` also queries the
database directly (README 117–126, 273). Every count and name in those claims is
right — only the pointers are off — but they are exactly the lines an examiner
would click through during a defense, and landing on eight `make_unique` calls
while looking for eight tool declarations is an avoidable stumble. Beyond those,
what remains is nitpick-grade: a missing `UK` marker on `students.user_id`, and
two endpoint descriptions that are correct for students but incomplete for staff.
