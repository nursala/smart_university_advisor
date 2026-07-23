# Auth module tests

This documents how to run the tests for the auth module (`AuthController`,
`UsersController`, `JwtAuthFilter`, `PasswordHasher`, `JwtService`,
`UserService`, `Base64`), and the real output from the actual runs used to
verify it (not hypothetical expected output).

## Unit tests (no server/DB needed)

`test/test_main.cc` adds 6 `DROGON_TEST` cases covering `PasswordHasher` and
`JwtService` in isolation (salting, verify success/failure on wrong password
and garbage input, issue/verify round-trip, signature tamper detection,
expiry, and the missing-`JWT_SECRET` constructor failure). Run them inside
the built `api` container:

```sh
docker compose up --build -d
docker compose exec api sh -c "cd /app/build/test && ./smart_university_advisor_test"
```

Real output:

```
================================================================================
  All tests passed (18 assertions in 8 tests cases).
```

(8 cases = the 2 pre-existing cases + the 6 new ones above.)

## Integration tests (real HTTP calls against the running stack)

With the stack up (`docker compose up -d`), the auth endpoints were exercised
end-to-end with real `curl` calls against `http://localhost:8080`. Summary of
what was verified (see the PR/audit notes for the full request/response
transcript of each case):

| Case | Result |
| --- | --- |
| `POST /auth/register`, new email | `201`, body has `id`/`name`/`email`/`role`/`token` |
| `POST /auth/register`, same email again | `400`, `"An account with this email already exists"` |
| `POST /auth/register`, missing password | `400` |
| `POST /auth/register`, 7-character password | `400` |
| `POST /auth/login`, correct password | `200` + `token` |
| `POST /auth/login`, wrong password | `400`, `"Invalid email or password"` |
| `POST /auth/login`, unregistered email | `400`, **same** `"Invalid email or password"` message (no user-enumeration leak) |
| `GET /users/me`, no `Authorization` header | `401` |
| `GET /users/me`, malformed header (`Authorization: garbage`) | `401` |
| `GET /users/me`, valid token | `200`, `id`/`email` match the registered account |
| `GET /users/me`, token with last character altered | `401` |
| `PATCH /users/me`, `{"name": "..."}` only | `200`, name updated, email unchanged (confirmed via follow-up `GET`) |
| `PATCH /users/me`, email already used by a different user | `400` |
| `PATCH /users/me`, empty body `{}` | `400` |
| 10 concurrent `POST /auth/register`, identical email | exactly one `201`, nine `400`s, no `500`s |

Reproduce with:

```sh
docker compose up --build -d
bash docs/auth-tests.sh   # prints the real request/response for every case above
```
