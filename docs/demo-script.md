# Grading demo script

1. Run `docker compose config`, then `docker compose up --build`.
2. Open `http://localhost:5173` and register a new student.
3. Show Profile defaults: Undeclared, year 1, 20 credits, GPA Not available.
4. Open Courses, inspect CS101, then inspect a nullable-instructor course.
5. In AI Advisor ask for a profile/summary/eligibility/plan review and show
   the ordered `tools_used` chain.
6. Ask “Enroll me in CS101 for 2028-Fall.” Show that the read-only advisor
   directs the student to My Plan and creates no enrollment.
7. Open My Plan, select an eligible course and canonical semester, then add it.
8. Show that the course appears once, leaves the eligible list, and returns
   after authorized removal.
9. Logout; sign in with the development demo student, then the admin account
   and show role-appropriate navigation.
10. Explain that live Gemini evidence requires a local `GEMINI_API_KEY`;
    never display the key or JWT.
