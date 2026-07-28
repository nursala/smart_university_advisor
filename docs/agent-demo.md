# Agent demonstration evidence

The AI Advisor is advisory and read-only. It exposes exactly eight tools:
`get_student_profile`, `get_academic_summary`, `get_available_courses`,
`get_course_recommendations`, `build_semester_plan`,
`analyze_academic_risk`, `get_course_details`, and `search_courses`.

Course enrollment is intentionally user-controlled because it changes academic
records. The AI recommends and previews plans, while the student performs the
final addition through My Plan.

## Live Gemini verification

**Evidence type: real Gemini.** Re-verified 2026-07-28 with
`gemini-3.1-flash-lite` against the docker-compose stack's seeded PostgreSQL
database (`docker compose up --build -d`). The API key, JWT, and
user/student identifiers are omitted.

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
  "answer": "Here is the review of your academic profile and the recommended plan:\n\n### Student Profile\n*   Department: Computer Science\n*   Year Level: 3\n*   Current GPA: 86.5\n\n### Academic Summary\n*   Completed Courses: 4 (CS101, CS102, CS201, CS202)\n*   Active Courses: 1 (CS302: Databases, 2026-Spring)\n*   Completed Credits: 16\n\n### Available Courses\nYou are eligible to take several Computer Science courses, including CS301 (Algorithms, Hard, 4 credits), CS303 (Operating Systems, Hard, 4 credits), CS304 (Web Development, Medium, 3 credits), CS305 (Software Engineering, Medium, 3 credits), CS306 (Computer Networks, Hard, 4 credits), and CS309 (Cyber Security, Medium, 3 credits).\n\n### Recommended Semester Plan (Max 12 Credits)\nTo balance your workload, three medium-difficulty courses totaling 9 credits: CS304, CS305, and CS309.\n\nTotal Credits: 9. Total Estimated Weekly Hours: 22.\n\nPlease note that I am unable to modify your academic records. To finalize these selections, please visit your \"My Plan\" page to add these courses to your schedule."
}
```

The enrollment count was `0` before and `0` after this read-only request (no
planned enrollments existed for the test student in either the before or
after check). Gemini produced four meaningful function calls, in the same
order as the tool chain documented above, followed by a final synthesized
answer—no repeated or out-of-order tool calls, and no attempt to call an
enrollment-mutating tool (none exist; all eight registered tools are
read-only). The loop stopped normally in five model rounds (four tool
rounds plus one final-answer round)—below `AgentLoop`'s six-round cap
(`kDefaultMaxToolRounds = 6`).

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
