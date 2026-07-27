# Concurrency test

This document provides reproducible concurrency checks for the current
authenticated API. Run the commands from the repository root in a POSIX shell
with Docker, `curl`, Node.js, and `jq` available.

The application uses four Drogon I/O threads by default, and the database pool
size is configurable through `DB_POOL_SIZE`.

## Setup: isolated student identity

Start the stack, register a temporary student, and extract the JWT-owned
identity without printing the token:

```sh
docker compose up --build -d

BASE_URL=http://localhost:8080
AUDIT_EMAIL="concurrency.$(date +%s)@example.com"
AUDIT_PASSWORD="concurrency-test-password"

AUTH_JSON=$(curl -fsS -X POST "$BASE_URL/auth/register" \
  -H "Content-Type: application/json" \
  -d "{\"name\":\"Concurrency Student\",\"email\":\"$AUDIT_EMAIL\",\"password\":\"$AUDIT_PASSWORD\"}")

TOKEN=$(printf '%s' "$AUTH_JSON" | jq -er '.token')
STUDENT_ID=$(printf '%s' "$AUTH_JSON" | jq -er '.user.student_id')
COURSE_ID=$(curl -fsS "$BASE_URL/courses" |
  jq -er '.[] | select(.code == "CS101") | .id' | head -n 1)
```

`POST /auth/register` and `GET /courses` are public. Every protected request
below includes `Authorization: Bearer $TOKEN`.

## Test 1: enrollment uniqueness race

The temporary student has no academic history, so seeded CS101 is eligible.
All 25 requests use the canonical semester `2028-Fall`. The request body
contains only `course_id` and `semester`; it does not send `student_id` or
`status`.

```sh
RACE_DIR=$(mktemp -d)

for i in $(seq 1 25); do
  curl -sS -o "$RACE_DIR/response_$i.json" -w "%{http_code}\n" \
    -X POST "$BASE_URL/enrollments" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "{\"course_id\":$COURSE_ID,\"semester\":\"2028-Fall\"}" \
    > "$RACE_DIR/status_$i.txt" &
done
wait

cat "$RACE_DIR"/status_*.txt | sort | uniq -c
```

Expected result for a clean run:

```text
      1 201
     24 409
```

Verify that the authenticated student's plan contains exactly one matching
row:

```sh
curl -fsS "$BASE_URL/enrollments/planned" \
  -H "Authorization: Bearer $TOKEN" |
  jq --argjson course_id "$COURSE_ID" \
    '[.[] | select(.course_id == $course_id and .semester == "2028-Fall")] | length'
```

The command must print `1`. Remove the winning planned enrollment through the
authorized API:

```sh
ENROLLMENT_ID=$(curl -fsS "$BASE_URL/enrollments/planned" \
  -H "Authorization: Bearer $TOKEN" |
  jq -er --argjson course_id "$COURSE_ID" \
    '.[] | select(.course_id == $course_id and .semester == "2028-Fall") | .id')

curl -fsS -X DELETE "$BASE_URL/enrollments/$ENROLLMENT_ID" \
  -H "Authorization: Bearer $TOKEN"

rm -rf "$RACE_DIR"
```

## Test 2: Agent request isolation with mocked Gemini

This section is **mocked Gemini concurrency evidence**, not a live Gemini test.
The real Drogon `/agent/query` route, JWT filter, Agent state, and Gemini HTTP
client execute normally, but `test/mock_gemini_server.js` supplies the model
response. The mock performs no tool calls in this scenario.

Start the mock and recreate only the API service with the local mock endpoint:

```sh
MOCK_GEMINI_PORT=5050 node test/mock_gemini_server.js \
  > /tmp/smart-university-mock-gemini.log 2>&1 &
MOCK_GEMINI_PID=$!

GEMINI_API_HOST=http://host.docker.internal:5050 \
GEMINI_API_KEY=mock-only-not-a-real-key \
GEMINI_MODEL=mock-model \
docker compose up --build -d --force-recreate api

# Recreating the API changes its process boot ID, so obtain a fresh JWT.
AUTH_JSON=$(curl -fsS -X POST "$BASE_URL/auth/login" \
  -H "Content-Type: application/json" \
  -d "{\"email\":\"$AUDIT_EMAIL\",\"password\":\"$AUDIT_PASSWORD\"}")
TOKEN=$(printf '%s' "$AUTH_JSON" | jq -er '.token')
STUDENT_ID=$(printf '%s' "$AUTH_JSON" | jq -er '.user.student_id')
```

Run 20 authenticated requests with distinct messages:

```sh
AGENT_DIR=$(mktemp -d)

for i in $(seq 1 20); do
  curl -fsS -o "$AGENT_DIR/response_$i.json" \
    -X POST "$BASE_URL/agent/query" \
    -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" \
    -d "{\"message\":\"concurrency message $i\"}" &
done
wait

for i in $(seq 1 20); do
  jq -e --arg expected "concurrency message $i" \
    --argjson student_id "$STUDENT_ID" \
    '.status == "ok" and .message == $expected and
     .student_id == $student_id and .tools_used == []' \
    "$AGENT_DIR/response_$i.json" > /dev/null
done

echo "All mocked Agent responses matched their request and JWT-owned student."
rm -rf "$AGENT_DIR"
```

Restore the normal Gemini configuration and stop the mock:

```sh
kill "$MOCK_GEMINI_PID"
docker compose up --build -d --force-recreate api
```

Live Gemini evidence is separate and is labeled **Live Gemini verification** in
`docs/agent-demo.md`. Do not present this mocked concurrency procedure as live
Gemini evidence.

## Cleanup

The enrollment is removed through the API above. The following local
development-only cleanup removes the temporary registered account:

```sh
docker compose exec -T db psql \
  -U "${DB_USER:-advisor}" \
  -d "${DB_NAME:-smart_university_advisor}" \
  -v email="$AUDIT_EMAIL" \
  -c "DELETE FROM users WHERE email = :'email';"
```

The Agent is read-only throughout both mocked and live operation. Students
create planned enrollments manually through My Plan, which sends only
`course_id` and a canonical `semester`; the backend supplies the authenticated
student identity.
