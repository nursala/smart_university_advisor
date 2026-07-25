# Grading demo script

1. Run `docker compose config`, then `docker compose up --build`.
2. Open `http://localhost:5173` and register a new student.
3. Show Profile defaults: Undeclared, year 1, 20 credits, GPA Not available.
4. Open Courses, inspect CS101, then inspect a nullable-instructor course.
5. On Dashboard request recommendations and build a 12-credit preview.
6. In AI Advisor ask for a profile/summary/eligibility/plan review and show
   the ordered `tools_used` chain.
7. Ask “Enroll me in CS101 for 2028-Fall.” Show that the confirmation card
   locks course and semester.
8. Cancel once to demonstrate no mutation; ask again and Confirm.
9. Open My Plan, show semester grouping and total credits, then remove CS101.
10. Logout; sign in with the development demo student, then the admin account
    and show role-appropriate navigation.
11. Explain that live Gemini evidence requires a local `GEMINI_API_KEY`;
    never display the key, JWT, or an unsanitized confirmation identifier.

