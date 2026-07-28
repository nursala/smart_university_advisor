# Authentication tests

This document covers the current authentication-related unit tests and the
manual HTTP exercise in `docs/auth-tests.sh`.

## C++ test suite

Run the complete suite inside the built API container:

```sh
docker compose up --build -d
docker compose exec -T api sh -c \
  "cd /app/build/test && ./smart_university_advisor_test"
```

Current verified output:

```text
================================================================================
All tests passed (199 assertions in 34 test cases).
```

Result: **34 test cases, 199 assertions, 0 failures**.

The suite includes password hashing and verification, the seeded demo password
hash, student/staff authorization rules, JWT issue/verify and rejection cases,
Agent student-identity scoping, exact eight-tool registration, semester
validation, and invalid enrollment/grade input handling.

## Manual HTTP authentication exercise

With the stack running, execute:

```sh
bash docs/auth-tests.sh
```

This is a manual HTTP exercise rather than an additional Drogon test-case
count. It covers:

| Case | Current route and expected behavior |
|---|---|
| Register | `POST /auth/register` returns `201` with `{ "token", "user" }` and linked `user.student_id` |
| Duplicate registration | `POST /auth/register` returns `400` |
| Invalid registration | Missing or short passwords return `400` |
| Login | `POST /auth/login` returns `200` with the same `{ "token", "user" }` contract |
| Invalid login | Wrong-password and unknown-email requests return the same error |
| Missing/malformed authentication | Protected `GET /users/me` returns `401` |
| Authenticated profile | `GET /users/me` includes `Authorization: Bearer <token>` and returns `200` |
| Tampered token | Protected `GET /users/me` returns `401` |
| Profile update | Protected `PATCH /users/me` includes the Bearer token |
| Duplicate email update | Protected `PATCH /users/me` returns `400` |
| Empty update | Protected `PATCH /users/me` returns `400` |
| Registration race | Ten concurrent public registrations produce one successful account |

`POST /auth/register` and `POST /auth/login` are public routes. Every successful
protected request in the script supplies the JWT using:

```sh
-H "Authorization: Bearer $TOKEN"
```

The script creates temporary development accounts and removes them from the
local Compose database at the end.

## Current Agent and enrollment boundary

Gemini exposes exactly eight read-only tools. It can read academic information,
recommend courses, analyze risk, and preview semester plans, but it does not
mutate enrollments.

Students add planned courses manually in My Plan. The frontend sends only
`course_id` and a semester in one of these canonical forms:

- `YYYY-Spring`
- `YYYY-Summer`
- `YYYY-Fall`
- `YYYY-Winter`

`POST /enrollments` is protected by JWT authentication and derives the student
identity from the authenticated account.
