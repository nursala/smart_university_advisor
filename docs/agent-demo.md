# Agent demonstration evidence

## Live Gemini verification

**Evidence type: real Gemini.** Verified with
`gemini-3.1-flash-lite` against a fresh isolated PostgreSQL database. The API
key, JWT, user/student IDs, course database ID, and confirmation identifier
are omitted.

Live planning request:

```http
POST /agent/query
Authorization: Bearer <redacted>
Content-Type: application/json

{
  "message": "First inspect my student profile, then review my academic summary, then find courses I am eligible to take, and finally build a balanced semester plan of at most 12 credits. Use the relevant tools for every step and synthesize the results."
}
```

Gemini called these tools in exact response order:

1. `get_student_profile`
2. `get_academic_summary`
3. `get_available_courses`
4. `build_semester_plan`

Sanitized live response:

```json
{
  "status": "ok",
  "student_id": "<redacted>",
  "tools_used": [
    "get_student_profile",
    "get_academic_summary",
    "get_available_courses",
    "build_semester_plan"
  ],
  "answer": "The student is a first-year Undeclared student with no completed credits or GPA yet. CS101 is currently eligible, and the synthesized plan contains CS101 for 4 credits and approximately 6 weekly hours, within the requested 12-credit maximum."
}
```

The enrollment count was `0` before and `0` after this read-only request.
Gemini produced four meaningful function calls followed by a final synthesized
answer, so the loop stopped normally in five model rounds—below the six-step
cap.

### Live enrollment verification

Live natural-language request:

```json
{"message":"Enroll me in CS101 for 2028-Fall."}
```

Gemini's ordered tools were:

1. `enroll_in_course`
2. `search_courses`
3. `enroll_in_course`

The initial enrollment attempt did not have the required numeric catalog
identity, so Gemini searched the catalog and retried with the resolved CS101
course. The final response contained:

```json
{
  "confirmation_id": "<redacted>",
  "course_id": "<redacted>",
  "course_code": "CS101",
  "course_name": "Introduction to Computer Science",
  "semester": "2028-Fall",
  "expires_in_seconds": 300,
  "status": "confirmation_required"
}
```

Database counts were:

| Checkpoint | Student enrollment count |
| --- | ---: |
| Before natural-language request | 0 |
| After proposal | 0 |
| After exact confirmation | 1 |
| After authorized removal | 0 |

Confirmation returned `success: true`. Replaying the same identifier returned
HTTP 403 with `Confirmation is invalid, expired, or already used`.
`GET /enrollments/planned` returned exactly one CS101 planned enrollment before
authorized removal, which returned `deleted: true`.

## Multi-tool planning scenario

**Evidence type: mocked Gemini.** The deterministic server in
`test/mock_gemini_server.js` exercised the real `AgentController`, real tool
registry, and fresh PostgreSQL database. A valid live Gemini key was not
available during the Batch 4 verification, so this transcript is not claimed
as live Gemini evidence.

Sanitized request:

```http
POST /agent/query
Authorization: Bearer <student-jwt>
Content-Type: application/json

{"message":"Review my situation and build a balanced plan."}
```

Sanitized response:

```json
{
  "student_id": 11,
  "status": "ok",
  "tools_used": [
    "get_student_profile",
    "get_academic_summary",
    "get_available_courses",
    "build_semester_plan"
  ],
  "answer": "Your profile and academic summary were reviewed. Eligible courses were checked, and a balanced plan within 12 credits was prepared from those results."
}
```

Each function call was returned to the same agent loop as a function response.
The controller performed five model round trips—four ordered tool calls and
one final answer—then stopped normally below its six-step cap. The four tools
are read-only; the student's enrollment count remained unchanged.

## Enrollment proposal and confirmation

**Evidence type: mocked Gemini.** The model adapter was mocked; proposal,
authorization, confirmation, enrollment, replay protection, My Plan, and
deletion used the real C++ and PostgreSQL implementation.

Natural-language request:

```text
Enroll me in CS101 for 2028-Fall.
```

Sanitized proposal:

```json
{
  "confirmation_id": "<redacted>",
  "course_id": 1,
  "course_code": "CS101",
  "course_name": "Introduction to Computer Science",
  "semester": "2028-Fall",
  "expires_in_seconds": 300,
  "status": "confirmation_required"
}
```

The fresh student's enrollment count was `0` before and after the proposal.
Submitting the exact identifier created one `planned` enrollment. Replaying
it returned HTTP 403 with `Confirmation is invalid, expired, or already
used`. `GET /enrollments/planned` returned exactly that CS101 enrollment;
authorized deletion succeeded and the enrollment count returned to `0`.

Confirmations are acquired and marked in-flight before `EnrollmentService`
runs. Success and academic validation failures consume them. An internal
service/database error releases the in-flight guard so the same unexpired
confirmation can be retried. Concurrent replay is rejected while processing.
