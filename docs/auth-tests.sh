#!/bin/bash
# Integration tests for the auth module (POST /auth/register, POST
# /auth/login, GET /users/me, PATCH /users/me), run against the live stack
# (docker compose up -d). Prints the real request/response for every case
# documented in docs/auth-tests.md, then deletes the test accounts it
# created.
set -u
BASE="http://localhost:8080"
EMAIL="audit.user.$(date +%s)@example.com"
EMAIL2="audit.user2.$(date +%s)@example.com"
PASS="correct-horse-battery"

echo "### TEST 1: POST /auth/register (valid new data) ###"
echo "REQUEST: POST $BASE/auth/register  body={\"name\":\"Audit User\",\"email\":\"$EMAIL\",\"password\":\"$PASS\"}"
RESP=$(curl -s -w "\n%{http_code}" -X POST "$BASE/auth/register" -H "Content-Type: application/json" \
  -d "{\"name\":\"Audit User\",\"email\":\"$EMAIL\",\"password\":\"$PASS\"}")
BODY=$(echo "$RESP" | head -n -1)
CODE=$(echo "$RESP" | tail -n1)
echo "RESPONSE ($CODE): $BODY"
TOKEN=$(echo "$BODY" | grep -o '"token":"[^"]*"' | cut -d'"' -f4)
USER_ID=$(echo "$BODY" | grep -o '"id":[0-9]*' | head -1 | grep -o '[0-9]*')
echo "extracted token=${TOKEN:0:20}... user_id=$USER_ID"
echo

echo "### TEST 2: POST /auth/register (same email again) ###"
RESP=$(curl -s -w "\n%{http_code}" -X POST "$BASE/auth/register" -H "Content-Type: application/json" \
  -d "{\"name\":\"Audit User Dup\",\"email\":\"$EMAIL\",\"password\":\"$PASS\"}")
BODY=$(echo "$RESP" | head -n -1); CODE=$(echo "$RESP" | tail -n1)
echo "RESPONSE ($CODE): $BODY"
echo

echo "### TEST 3: POST /auth/register (missing password) ###"
RESP=$(curl -s -w "\n%{http_code}" -X POST "$BASE/auth/register" -H "Content-Type: application/json" \
  -d "{\"name\":\"No Pass\",\"email\":\"nopass.$(date +%s)@example.com\"}")
BODY=$(echo "$RESP" | head -n -1); CODE=$(echo "$RESP" | tail -n1)
echo "RESPONSE ($CODE): $BODY"
echo

echo "### TEST 4: POST /auth/register (7-char password) ###"
RESP=$(curl -s -w "\n%{http_code}" -X POST "$BASE/auth/register" -H "Content-Type: application/json" \
  -d "{\"name\":\"Short Pass\",\"email\":\"shortpass.$(date +%s)@example.com\",\"password\":\"1234567\"}")
BODY=$(echo "$RESP" | head -n -1); CODE=$(echo "$RESP" | tail -n1)
echo "RESPONSE ($CODE): $BODY"
echo

echo "### TEST 5: POST /auth/login (correct credentials) ###"
RESP=$(curl -s -w "\n%{http_code}" -X POST "$BASE/auth/login" -H "Content-Type: application/json" \
  -d "{\"email\":\"$EMAIL\",\"password\":\"$PASS\"}")
BODY=$(echo "$RESP" | head -n -1); CODE=$(echo "$RESP" | tail -n1)
echo "RESPONSE ($CODE): $BODY"
echo

echo "### TEST 6: POST /auth/login (right email, wrong password) ###"
RESP=$(curl -s -w "\n%{http_code}" -X POST "$BASE/auth/login" -H "Content-Type: application/json" \
  -d "{\"email\":\"$EMAIL\",\"password\":\"wrong-password\"}")
BODY=$(echo "$RESP" | head -n -1); CODE=$(echo "$RESP" | tail -n1)
echo "RESPONSE ($CODE): $BODY"
WRONGPW_MSG="$BODY"
echo

echo "### TEST 7: POST /auth/login (never-registered email) ###"
RESP=$(curl -s -w "\n%{http_code}" -X POST "$BASE/auth/login" -H "Content-Type: application/json" \
  -d "{\"email\":\"never.registered.$(date +%s)@example.com\",\"password\":\"whatever123\"}")
BODY=$(echo "$RESP" | head -n -1); CODE=$(echo "$RESP" | tail -n1)
echo "RESPONSE ($CODE): $BODY"
NOEMAIL_MSG="$BODY"
echo
echo "Generic-message check: wrong-password body == unknown-email body? -> $([ "$WRONGPW_MSG" == "$NOEMAIL_MSG" ] && echo YES-MATCH || echo MISMATCH)"
echo

echo "### TEST 8: GET /users/me (no Authorization header) ###"
RESP=$(curl -s -w "\n%{http_code}" "$BASE/users/me")
BODY=$(echo "$RESP" | head -n -1); CODE=$(echo "$RESP" | tail -n1)
echo "RESPONSE ($CODE): $BODY"
echo

echo "### TEST 9: GET /users/me (malformed Authorization header) ###"
RESP=$(curl -s -w "\n%{http_code}" -H "Authorization: garbage" "$BASE/users/me")
BODY=$(echo "$RESP" | head -n -1); CODE=$(echo "$RESP" | tail -n1)
echo "RESPONSE ($CODE): $BODY"
echo

echo "### TEST 10: GET /users/me (real token) ###"
RESP=$(curl -s -w "\n%{http_code}" -H "Authorization: Bearer $TOKEN" "$BASE/users/me")
BODY=$(echo "$RESP" | head -n -1); CODE=$(echo "$RESP" | tail -n1)
echo "RESPONSE ($CODE): $BODY"
echo "Expected id=$USER_ID email=$EMAIL"
echo

echo "### TEST 11: GET /users/me (token with last char altered) ###"
BAD_TOKEN="${TOKEN%?}X"
if [ "${TOKEN: -1}" == "X" ]; then BAD_TOKEN="${TOKEN%?}Y"; fi
RESP=$(curl -s -w "\n%{http_code}" -H "Authorization: Bearer $BAD_TOKEN" "$BASE/users/me")
BODY=$(echo "$RESP" | head -n -1); CODE=$(echo "$RESP" | tail -n1)
echo "RESPONSE ($CODE): $BODY"
echo

echo "### TEST 12: PATCH /users/me ({\"name\":...} only) then GET /users/me ###"
RESP=$(curl -s -w "\n%{http_code}" -X PATCH "$BASE/users/me" -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" -d '{"name":"Audit User Renamed"}')
BODY=$(echo "$RESP" | head -n -1); CODE=$(echo "$RESP" | tail -n1)
echo "PATCH RESPONSE ($CODE): $BODY"
RESP2=$(curl -s -w "\n%{http_code}" -H "Authorization: Bearer $TOKEN" "$BASE/users/me")
BODY2=$(echo "$RESP2" | head -n -1); CODE2=$(echo "$RESP2" | tail -n1)
echo "Follow-up GET RESPONSE ($CODE2): $BODY2"
echo

echo "### Setup: register a second user for the email-collision test ###"
RESP=$(curl -s -w "\n%{http_code}" -X POST "$BASE/auth/register" -H "Content-Type: application/json" \
  -d "{\"name\":\"Second User\",\"email\":\"$EMAIL2\",\"password\":\"another-password\"}")
BODY=$(echo "$RESP" | head -n -1); CODE=$(echo "$RESP" | tail -n1)
echo "RESPONSE ($CODE): $BODY"
echo

echo "### TEST 13: PATCH /users/me (email already used by a DIFFERENT user) ###"
RESP=$(curl -s -w "\n%{http_code}" -X PATCH "$BASE/users/me" -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" -d "{\"email\":\"$EMAIL2\"}")
BODY=$(echo "$RESP" | head -n -1); CODE=$(echo "$RESP" | tail -n1)
echo "RESPONSE ($CODE): $BODY"
echo

echo "### TEST 14: PATCH /users/me (empty JSON body {}) ###"
RESP=$(curl -s -w "\n%{http_code}" -X PATCH "$BASE/users/me" -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" -d '{}')
BODY=$(echo "$RESP" | head -n -1); CODE=$(echo "$RESP" | tail -n1)
echo "RESPONSE ($CODE): $BODY"
echo

echo "### TEST 15: Concurrency -- 10 concurrent POST /auth/register, same email ###"
RACE_EMAIL="audit.race.$(date +%s)@example.com"
RACE_DIR=$(mktemp -d)
for i in $(seq 1 10); do
  curl -s -o "$RACE_DIR/resp_$i.json" -w "%{http_code}\n" -X POST "$BASE/auth/register" \
    -H "Content-Type: application/json" \
    -d "{\"name\":\"Racer $i\",\"email\":\"$RACE_EMAIL\",\"password\":\"race-password\"}" \
    > "$RACE_DIR/status_$i.txt" &
done
wait
echo "Status code distribution:"
cat "$RACE_DIR"/status_*.txt | sort | uniq -c
echo "The one 201 body:"
grep -l 201 "$RACE_DIR"/status_*.txt | sed "s#status_#resp_#;s#.txt#.json#" | xargs -r cat
rm -rf "$RACE_DIR"
echo

echo "### Cleanup: removing test accounts created by this script ###"
docker compose exec -T db psql -U "${DB_USER:-advisor}" -d "${DB_NAME:-smart_university_advisor}" -c \
  "DELETE FROM users WHERE email LIKE 'audit.%@example.com' OR email LIKE 'nopass.%@example.com' OR email LIKE 'shortpass.%@example.com' OR email LIKE 'never.registered.%@example.com';"
