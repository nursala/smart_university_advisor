# Grading demo script

1. Run `docker compose config`, then `docker compose up --build -d`.
2. Open `http://localhost:5173` and register a new student.
3. Show the new student's Profile defaults: department `Undeclared`, year `1`,
   maximum weekly credits `20`, and GPA `Not available`.
4. Open Courses, inspect CS101, and show its credits, difficulty, assigned
   instructor, description, and prerequisite information.
5. Open AI Advisor and ask for a profile, academic-summary, eligibility, and
   semester-plan review. Show the ordered `tools_used` chain and the final
   synthesized answer.
6. Explain that Gemini exposes exactly eight read-only tools. It can recommend
   courses and preview semester plans, but it cannot create, update, or delete
   enrollments.
7. Ask: "Enroll me in CS101 for 2028-Fall." Show that the advisor directs the
   student to My Plan and does not change the enrollment list.
8. Open My Plan, select an eligible course, select `2028-Fall`, and add it
   manually. The frontend sends only `course_id` and `semester`; the backend
   derives the student identity from the authenticated JWT.
9. Show that the new planned course appears once and leaves the eligible-course
   list. Remove it, wait for the refresh, and show that it becomes eligible
   again when academic rules permit.
10. Log out. Sign in with the development demo student and show Profile,
    Courses, and role-appropriate navigation. Sign in with the development
    administrator only when demonstrating a documented staff operation.
11. Use the evidence in `docs/agent-demo.md`:
    - the deterministic mock transcript is labeled **Mocked regression
      evidence**;
    - the real-provider transcript is labeled **Live Gemini verification**.
      Do not describe mocked output as live Gemini output.

Live Gemini requires a valid local `GEMINI_API_KEY`. Never display the key or
JWT during the demonstration.
