# Concurrency test

This documents real load tests run against the rebuilt, multi-threaded stack
(not hypothetical/"provably safe" reasoning). All commands and output below
are copied verbatim from the actual test run.

## Config under test

- `config.json`: `number_of_threads: 4` (was `1`)
- `main.cc`: DB connection-pool size is now `DB_POOL_SIZE` (env, default `4`,
  matching the thread count) instead of a hardcoded `2`
- `docker-compose.yml`'s `api` service passes through `DB_POOL_SIZE` and
  `GEMINI_API_HOST` (both additive, both defaulted to unchanged behavior)

## Why a mock Gemini server

No real `GEMINI_API_KEY` is configured in this environment (`.env.example`
ships it empty, and no `.env` file exists in the repo). `GeminiClient`'s
constructor throws immediately without one (`services/GeminiClient.cc`), so
a real end-to-end `/agent/query` call against the actual Gemini API isn't
possible here. To still genuinely load-test `/agent/query`'s per-request
state isolation under real parallelism (not skip the test), `GeminiClient`
gained a `GEMINI_API_HOST` env override (default: the real Gemini host, so
production behavior is unchanged — see `services/GeminiClient.h/.cc`) and
`test/mock_gemini_server.js` stands in for Gemini's `generateContent`
endpoint: it parses the `student_id` embedded in `AgentController`'s prompt
text and echoes it back as the model's final answer (no tool calls), so
each request completes in one real round trip through the actual
4-IO-thread Drogon server and the actual `shared_ptr<AgentState>` per-request
plumbing — only the Gemini call itself is faked. This substitution is
reported explicitly, per the same posture as Batch 1's Gemini-path
substitution in `test/test_main.cc`.

## Test 1: Enrollment uniqueness race (25 concurrent identical `POST /enrollments`)

Command:

```sh
mkdir -p /tmp/enroll_race
for i in $(seq 1 25); do
  curl -s -o "/tmp/enroll_race/resp_$i.json" -w "%{http_code}\n" -X POST http://localhost:8080/enrollments \
    -H "Content-Type: application/json" \
    -d '{"student_id": 1, "course_id": 1, "semester": "2027-ConcurrencyTest-2"}' \
    > "/tmp/enroll_race/status_$i.txt" &
done
wait
cat /tmp/enroll_race/status_*.txt | sort | uniq -c
```

Real output:

```
      1 201
     24 409
```

The winning `201` body:

```json
{"course_id":1,"enrolled_at":"2026-07-22 13:50:55.440731","id":70,"semester":"2027-ConcurrencyTest-2","status":"planned","student_id":1}
```

A sample `409` body (identical for all 24 losers):

```json
{"error":"Enrollment already exists for this student, course, and semester"}
```

Server survival + row-count check:

```sh
curl -s -w "\n%{http_code}\n" http://localhost:8080/students/1/profile
```
```
{"current_gpa":86.5,...,"student_number":"S2026001","year_level":3}
200
```

```sh
docker compose exec db psql -U advisor -d smart_university_advisor -c \
  "SELECT count(*) FROM enrollments WHERE student_id = 1 AND course_id = 1 AND semester LIKE '2027-ConcurrencyTest%';"
```
```
 count
-------
     2
(1 row)
```

(2, not 1, because this test was run twice against two distinct semester
strings — `2027-ConcurrencyTest` and `2027-ConcurrencyTest-2` — while
iterating on the verification command itself; each run independently
produced exactly one row for its own semester, i.e. the uniqueness
constraint held on both runs, not just one.)

Test rows were deleted afterward:

```sh
docker compose exec db psql -U advisor -d smart_university_advisor -c \
  "DELETE FROM enrollments WHERE student_id = 1 AND course_id = 1 AND semester LIKE '2027-ConcurrencyTest%';"
```
```
DELETE 2
```

**Result: no crash, exactly one winner per burst, no duplicate rows, no partial state.**

## Test 2: `/agent/query` cross-talk under real concurrency

Setup — mock server, then the `api` container rebuilt/restarted pointing at it:

```sh
node test/mock_gemini_server.js &
docker compose stop api
GEMINI_API_HOST=http://host.docker.internal:5050 GEMINI_API_KEY=test-key GEMINI_MODEL=test-model \
  docker compose up --build -d api
```

Sanity check (single request):

```sh
curl -s -w "\n%{http_code}\n" -X POST http://localhost:8080/agent/query \
  -H "Content-Type: application/json" -d '{"student_id": 7, "message": "test message 7"}'
```
```
{"answer":"Echo: 7","message":"test message 7","status":"ok","student_id":7,"tools_used":[]}
200
```

### 10 concurrent requests, distinct `student_id`s

```sh
for sid in $(seq 1 10); do
  curl -s -o "/tmp/agent_concurrency/resp_$sid.json" -X POST http://localhost:8080/agent/query \
    -H "Content-Type: application/json" \
    -d "{\"student_id\": $sid, \"message\": \"test message $sid\"}" &
done
wait
```

Real responses (`requested=<sid sent> -> <body received>`):

```
requested=1 -> {"answer":"Echo: 1","message":"test message 1","status":"ok","student_id":1,"tools_used":[]}
requested=2 -> {"answer":"Echo: 2","message":"test message 2","status":"ok","student_id":2,"tools_used":[]}
requested=3 -> {"answer":"Echo: 3","message":"test message 3","status":"ok","student_id":3,"tools_used":[]}
requested=4 -> {"answer":"Echo: 4","message":"test message 4","status":"ok","student_id":4,"tools_used":[]}
requested=5 -> {"answer":"Echo: 5","message":"test message 5","status":"ok","student_id":5,"tools_used":[]}
requested=6 -> {"answer":"Echo: 6","message":"test message 6","status":"ok","student_id":6,"tools_used":[]}
requested=7 -> {"answer":"Echo: 7","message":"test message 7","status":"ok","student_id":7,"tools_used":[]}
requested=8 -> {"answer":"Echo: 8","message":"test message 8","status":"ok","student_id":8,"tools_used":[]}
requested=9 -> {"answer":"Echo: 9","message":"test message 9","status":"ok","student_id":9,"tools_used":[]}
requested=10 -> {"answer":"Echo: 10","message":"test message 10","status":"ok","student_id":10,"tools_used":[]}
```

Every response's `student_id` and `answer` match the request that produced
it — no cross-talk.

### 20 concurrent requests, distinct `student_id`s, timed

```sh
time (
for sid in $(seq 1 20); do
  curl -s -o "/tmp/agent_concurrency/resp_$sid.json" -X POST http://localhost:8080/agent/query \
    -H "Content-Type: application/json" \
    -d "{\"student_id\": $sid, \"message\": \"test message $sid\"}" &
done
wait
)
```

Real output:

```
real    0m0.836s
user    0m0.136s
sys     0m0.167s
```

All 20 responses matched their own request's `student_id` and expected
`Echo: <sid>` answer (verified programmatically, `mismatch flag: 0`). The
mock server's per-request delay is randomized 50–250ms; 20 requests
completing in 0.836s wall time (rather than the ~1.5–3s a serialized
50–250ms-per-request chain would take) is consistent with the 4 requests
genuinely overlapping in flight across the 4 IO threads, not a
single-threaded callback chain processing them one at a time.

Cleanup — mock server killed, `api` container rebuilt back onto the default
(real) Gemini host:

```sh
docker compose stop api
docker compose up --build -d api
docker compose exec api sh -c 'echo $GEMINI_API_HOST'
```
```
https://generativelanguage.googleapis.com
```

**Result: no cross-talk across 30 total concurrent requests (10 + 20) with
distinct `student_id`s — `shared_ptr<AgentState>` isolation held under real
multi-threaded parallelism.**

## Summary

| Test | Config | Result |
| --- | --- | --- |
| 25x concurrent identical `POST /enrollments` | 4 threads, DB pool 4 | Exactly 1×201, 24×409, no crash, no duplicate row (confirmed twice) |
| 10x concurrent `/agent/query`, distinct `student_id` | 4 threads, mock Gemini | 0 cross-talk |
| 20x concurrent `/agent/query`, distinct `student_id` | 4 threads, mock Gemini | 0 cross-talk, 0.836s wall time (consistent with real overlap) |

Nothing broke under real concurrency in this run.
