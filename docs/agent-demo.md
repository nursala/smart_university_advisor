# Agent demonstration evidence

The AI Advisor is advisory and read-only. It exposes exactly eight tools:
`get_student_profile`, `get_academic_summary`, `get_available_courses`,
`get_course_recommendations`, `build_semester_plan`,
`analyze_academic_risk`, `get_course_details`, and `search_courses`.

Course enrollment is intentionally user-controlled because it changes academic
records. The AI recommends and previews plans, while the student performs the
final addition through My Plan.

## Live Gemini verification

**Evidence type: real Gemini.** Verified with
`gemini-3.1-flash-lite` against a fresh isolated PostgreSQL database. The API
key, JWT, and user/student identifiers are omitted.

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

## Multi-tool planning scenario

**Evidence type: mocked Gemini.** The deterministic server in
`test/mock_gemini_server.js` exercised the real `AgentController`, real tool
registry, and fresh PostgreSQL database. This regression evidence remains
separately labeled and is not claimed as live Gemini evidence.

Sanitized response:

```json
{
  "student_id": "<redacted>",
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
one final answer—then stopped normally below its six-step cap. Recommendation,
planning, and all other registered tools are read-only; enrollment counts
remain unchanged.

## Manual My Plan workflow

Students create planned enrollments only on My Plan. The authenticated UI
loads both planned enrollments and eligible courses, then submits only
`course_id` and `semester` to `POST /enrollments`. The server derives
`student_id` from the JWT and applies prerequisite, repeat, duplicate,
semester, and credit-limit rules transactionally. After add or remove, both
lists refresh so eligibility changes are immediately visible.
