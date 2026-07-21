INSERT INTO users (name, email, password_hash, role)
VALUES
('Admin User', 'admin@example.com', '$2b$12$GxQqY18zWbYkNqO8qH7u/.bqLmVoeI6gqgSAq2m6R5j0I6byQpJ8K', 'admin'),
('Academic Advisor', 'advisor@example.com', '$2b$12$GxQqY18zWbYkNqO8qH7u/.bqLmVoeI6gqgSAq2m6R5j0I6byQpJ8K', 'advisor'),
('Senior Advisor', 'senior.advisor@example.com', '$2b$12$GxQqY18zWbYkNqO8qH7u/.bqLmVoeI6gqgSAq2m6R5j0I6byQpJ8K', 'advisor'),
('Adam Cohen', 'adam@example.com', '$2b$12$GxQqY18zWbYkNqO8qH7u/.bqLmVoeI6gqgSAq2m6R5j0I6byQpJ8K', 'student'),
('Sara Levi', 'sara@example.com', '$2b$12$GxQqY18zWbYkNqO8qH7u/.bqLmVoeI6gqgSAq2m6R5j0I6byQpJ8K', 'student'),
('Noa Mizrahi', 'noa@example.com', '$2b$12$GxQqY18zWbYkNqO8qH7u/.bqLmVoeI6gqgSAq2m6R5j0I6byQpJ8K', 'student'),
('Daniel Peretz', 'daniel@example.com', '$2b$12$GxQqY18zWbYkNqO8qH7u/.bqLmVoeI6gqgSAq2m6R5j0I6byQpJ8K', 'student'),
('Maya Rosen', 'maya.student@example.com', '$2b$12$GxQqY18zWbYkNqO8qH7u/.bqLmVoeI6gqgSAq2m6R5j0I6byQpJ8K', 'student'),
('Omer Hadad', 'omer@example.com', '$2b$12$GxQqY18zWbYkNqO8qH7u/.bqLmVoeI6gqgSAq2m6R5j0I6byQpJ8K', 'student'),
('Lian Bar', 'lian@example.com', '$2b$12$GxQqY18zWbYkNqO8qH7u/.bqLmVoeI6gqgSAq2m6R5j0I6byQpJ8K', 'student'),
('Yaron Shalev', 'yaron@example.com', '$2b$12$GxQqY18zWbYkNqO8qH7u/.bqLmVoeI6gqgSAq2m6R5j0I6byQpJ8K', 'student'),
('Tamar Azulay', 'tamar@example.com', '$2b$12$GxQqY18zWbYkNqO8qH7u/.bqLmVoeI6gqgSAq2m6R5j0I6byQpJ8K', 'student'),
('Eitan Mor', 'eitan@example.com', '$2b$12$GxQqY18zWbYkNqO8qH7u/.bqLmVoeI6gqgSAq2m6R5j0I6byQpJ8K', 'student');

INSERT INTO instructors (name, email, department, rating)
VALUES
('Dr. David Rosen', 'david.rosen@example.com', 'Computer Science', 4.5),
('Dr. Maya Klein', 'maya.klein@example.com', 'Computer Science', 4.2),
('Prof. Amir Haddad', 'amir.haddad@example.com', 'Computer Science', 4.7),
('Dr. Leora Stein', 'leora.stein@example.com', 'Computer Science', 4.1),
('Prof. Gil Barak', 'gil.barak@example.com', 'Computer Science', 4.6);

INSERT INTO students
(user_id, student_number, department, year_level, current_gpa, max_weekly_credits)
VALUES
((SELECT id FROM users WHERE email = 'adam@example.com'), 'S2026001', 'Computer Science', 3, 86.50, 18),
((SELECT id FROM users WHERE email = 'sara@example.com'), 'S2026002', 'Computer Science', 2, 58.20, 16),
((SELECT id FROM users WHERE email = 'noa@example.com'), 'S2026003', 'Computer Science', 2, 78.40, 18),
((SELECT id FROM users WHERE email = 'daniel@example.com'), 'S2026004', 'Computer Science', 3, 91.00, 20),
((SELECT id FROM users WHERE email = 'maya.student@example.com'), 'S2026005', 'Computer Science', 1, 72.00, 16),
((SELECT id FROM users WHERE email = 'omer@example.com'), 'S2026006', 'Computer Science', 4, 83.70, 18),
((SELECT id FROM users WHERE email = 'lian@example.com'), 'S2026007', 'Computer Science', 2, 66.30, 14),
((SELECT id FROM users WHERE email = 'yaron@example.com'), 'S2026008', 'Computer Science', 3, 74.80, 16),
((SELECT id FROM users WHERE email = 'tamar@example.com'), 'S2026009', 'Computer Science', 1, 88.10, 18),
((SELECT id FROM users WHERE email = 'eitan@example.com'), 'S2026010', 'Computer Science', 4, 79.20, 20);

INSERT INTO courses
(code, name, department, credits, difficulty_level, estimated_weekly_hours, description, instructor_id)
VALUES
('CS101', 'Introduction to Computer Science', 'Computer Science', 4, 'easy', 6, 'Basic concepts of computer science and computational thinking.', (SELECT id FROM instructors WHERE email = 'david.rosen@example.com')),
('CS102', 'Programming Fundamentals', 'Computer Science', 4, 'easy', 7, 'Programming basics, control flow, functions, and problem solving.', (SELECT id FROM instructors WHERE email = 'maya.klein@example.com')),
('CS201', 'Object Oriented Programming', 'Computer Science', 4, 'medium', 8, 'Object oriented principles, design, inheritance, and polymorphism.', (SELECT id FROM instructors WHERE email = 'maya.klein@example.com')),
('CS202', 'Data Structures', 'Computer Science', 4, 'medium', 9, 'Lists, stacks, queues, trees, hash tables, and graphs.', (SELECT id FROM instructors WHERE email = 'amir.haddad@example.com')),
('CS301', 'Algorithms', 'Computer Science', 4, 'hard', 12, 'Algorithm design, analysis, complexity, and correctness.', (SELECT id FROM instructors WHERE email = 'amir.haddad@example.com')),
('CS302', 'Databases', 'Computer Science', 4, 'medium', 8, 'Relational modeling, SQL, transactions, and database design.', (SELECT id FROM instructors WHERE email = 'david.rosen@example.com')),
('CS303', 'Operating Systems', 'Computer Science', 4, 'hard', 11, 'Processes, threads, scheduling, memory, and file systems.', (SELECT id FROM instructors WHERE email = 'leora.stein@example.com')),
('CS304', 'Web Development', 'Computer Science', 3, 'medium', 7, 'Backend and frontend foundations for modern web applications.', (SELECT id FROM instructors WHERE email = 'gil.barak@example.com')),
('CS305', 'Software Engineering', 'Computer Science', 3, 'medium', 7, 'Software lifecycle, requirements, design, testing, and teamwork.', (SELECT id FROM instructors WHERE email = 'gil.barak@example.com')),
('CS306', 'Computer Networks', 'Computer Science', 4, 'hard', 10, 'Network architecture, protocols, routing, and transport layers.', (SELECT id FROM instructors WHERE email = 'leora.stein@example.com')),
('CS307', 'Artificial Intelligence', 'Computer Science', 4, 'hard', 11, 'Search, knowledge representation, planning, and intelligent agents.', (SELECT id FROM instructors WHERE email = 'amir.haddad@example.com')),
('CS308', 'Machine Learning', 'Computer Science', 4, 'hard', 12, 'Supervised and unsupervised learning, model evaluation, and pipelines.', (SELECT id FROM instructors WHERE email = 'amir.haddad@example.com')),
('CS309', 'Cyber Security', 'Computer Science', 3, 'medium', 8, 'Security principles, vulnerabilities, cryptography, and secure systems.', (SELECT id FROM instructors WHERE email = 'leora.stein@example.com')),
('CS310', 'Cloud Computing', 'Computer Science', 3, 'medium', 8, 'Cloud platforms, containers, distributed deployment, and scalability.', (SELECT id FROM instructors WHERE email = 'gil.barak@example.com')),
('CS311', 'Distributed Systems', 'Computer Science', 4, 'hard', 12, 'Distributed coordination, consistency, replication, and fault tolerance.', (SELECT id FROM instructors WHERE email = 'david.rosen@example.com'));

INSERT INTO course_prerequisites (course_id, prerequisite_course_id, minimum_grade)
VALUES
((SELECT id FROM courses WHERE code = 'CS102'), (SELECT id FROM courses WHERE code = 'CS101'), 60),
((SELECT id FROM courses WHERE code = 'CS201'), (SELECT id FROM courses WHERE code = 'CS102'), 60),
((SELECT id FROM courses WHERE code = 'CS202'), (SELECT id FROM courses WHERE code = 'CS201'), 60),
((SELECT id FROM courses WHERE code = 'CS301'), (SELECT id FROM courses WHERE code = 'CS202'), 70),
((SELECT id FROM courses WHERE code = 'CS302'), (SELECT id FROM courses WHERE code = 'CS101'), 60),
((SELECT id FROM courses WHERE code = 'CS303'), (SELECT id FROM courses WHERE code = 'CS202'), 60),
((SELECT id FROM courses WHERE code = 'CS304'), (SELECT id FROM courses WHERE code = 'CS102'), 60),
((SELECT id FROM courses WHERE code = 'CS305'), (SELECT id FROM courses WHERE code = 'CS201'), 60),
((SELECT id FROM courses WHERE code = 'CS306'), (SELECT id FROM courses WHERE code = 'CS202'), 60),
((SELECT id FROM courses WHERE code = 'CS307'), (SELECT id FROM courses WHERE code = 'CS301'), 65),
((SELECT id FROM courses WHERE code = 'CS308'), (SELECT id FROM courses WHERE code = 'CS301'), 70),
((SELECT id FROM courses WHERE code = 'CS309'), (SELECT id FROM courses WHERE code = 'CS202'), 60),
((SELECT id FROM courses WHERE code = 'CS310'), (SELECT id FROM courses WHERE code = 'CS302'), 60),
((SELECT id FROM courses WHERE code = 'CS311'), (SELECT id FROM courses WHERE code = 'CS303'), 60),
((SELECT id FROM courses WHERE code = 'CS311'), (SELECT id FROM courses WHERE code = 'CS306'), 60);

INSERT INTO enrollments (student_id, course_id, semester, status)
VALUES
((SELECT id FROM students WHERE student_number = 'S2026001'), (SELECT id FROM courses WHERE code = 'CS101'), '2024A', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026001'), (SELECT id FROM courses WHERE code = 'CS102'), '2024B', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026001'), (SELECT id FROM courses WHERE code = 'CS201'), '2025A', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026001'), (SELECT id FROM courses WHERE code = 'CS202'), '2025B', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026001'), (SELECT id FROM courses WHERE code = 'CS302'), '2026A', 'active'),
((SELECT id FROM students WHERE student_number = 'S2026002'), (SELECT id FROM courses WHERE code = 'CS101'), '2025A', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026002'), (SELECT id FROM courses WHERE code = 'CS102'), '2025B', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026002'), (SELECT id FROM courses WHERE code = 'CS201'), '2026A', 'active'),
((SELECT id FROM students WHERE student_number = 'S2026003'), (SELECT id FROM courses WHERE code = 'CS101'), '2025A', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026003'), (SELECT id FROM courses WHERE code = 'CS102'), '2025B', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026003'), (SELECT id FROM courses WHERE code = 'CS201'), '2026A', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026003'), (SELECT id FROM courses WHERE code = 'CS304'), '2026B', 'planned'),
((SELECT id FROM students WHERE student_number = 'S2026004'), (SELECT id FROM courses WHERE code = 'CS101'), '2023A', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026004'), (SELECT id FROM courses WHERE code = 'CS102'), '2023B', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026004'), (SELECT id FROM courses WHERE code = 'CS201'), '2024A', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026004'), (SELECT id FROM courses WHERE code = 'CS202'), '2024B', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026004'), (SELECT id FROM courses WHERE code = 'CS301'), '2025A', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026004'), (SELECT id FROM courses WHERE code = 'CS303'), '2025B', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026004'), (SELECT id FROM courses WHERE code = 'CS308'), '2026A', 'active'),
((SELECT id FROM students WHERE student_number = 'S2026005'), (SELECT id FROM courses WHERE code = 'CS101'), '2026A', 'active'),
((SELECT id FROM students WHERE student_number = 'S2026005'), (SELECT id FROM courses WHERE code = 'CS102'), '2026B', 'planned'),
((SELECT id FROM students WHERE student_number = 'S2026006'), (SELECT id FROM courses WHERE code = 'CS101'), '2023A', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026006'), (SELECT id FROM courses WHERE code = 'CS102'), '2023B', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026006'), (SELECT id FROM courses WHERE code = 'CS201'), '2024A', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026006'), (SELECT id FROM courses WHERE code = 'CS202'), '2024B', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026006'), (SELECT id FROM courses WHERE code = 'CS301'), '2025A', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026006'), (SELECT id FROM courses WHERE code = 'CS302'), '2025B', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026006'), (SELECT id FROM courses WHERE code = 'CS303'), '2026A', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026006'), (SELECT id FROM courses WHERE code = 'CS306'), '2026A', 'active'),
((SELECT id FROM students WHERE student_number = 'S2026007'), (SELECT id FROM courses WHERE code = 'CS101'), '2025A', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026007'), (SELECT id FROM courses WHERE code = 'CS102'), '2025B', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026007'), (SELECT id FROM courses WHERE code = 'CS201'), '2026A', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026007'), (SELECT id FROM courses WHERE code = 'CS202'), '2026B', 'planned'),
((SELECT id FROM students WHERE student_number = 'S2026008'), (SELECT id FROM courses WHERE code = 'CS101'), '2024A', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026008'), (SELECT id FROM courses WHERE code = 'CS102'), '2024B', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026008'), (SELECT id FROM courses WHERE code = 'CS201'), '2025A', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026008'), (SELECT id FROM courses WHERE code = 'CS202'), '2025B', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026008'), (SELECT id FROM courses WHERE code = 'CS301'), '2026A', 'active'),
((SELECT id FROM students WHERE student_number = 'S2026009'), (SELECT id FROM courses WHERE code = 'CS101'), '2026A', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026010'), (SELECT id FROM courses WHERE code = 'CS101'), '2023A', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026010'), (SELECT id FROM courses WHERE code = 'CS102'), '2023B', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026010'), (SELECT id FROM courses WHERE code = 'CS201'), '2024A', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026010'), (SELECT id FROM courses WHERE code = 'CS202'), '2024B', 'completed'),
((SELECT id FROM students WHERE student_number = 'S2026010'), (SELECT id FROM courses WHERE code = 'CS302'), '2025A', 'completed');

INSERT INTO grades (enrollment_id, grade, passed)
SELECT e.id, grade_data.grade, grade_data.grade >= 60
FROM (
    VALUES
    ('S2026001', 'CS101', '2024A', 91.00),
    ('S2026001', 'CS102', '2024B', 88.00),
    ('S2026001', 'CS201', '2025A', 85.00),
    ('S2026001', 'CS202', '2025B', 82.00),
    ('S2026002', 'CS101', '2025A', 62.00),
    ('S2026002', 'CS102', '2025B', 55.00),
    ('S2026003', 'CS101', '2025A', 81.00),
    ('S2026003', 'CS102', '2025B', 77.00),
    ('S2026003', 'CS201', '2026A', 74.00),
    ('S2026004', 'CS101', '2023A', 96.00),
    ('S2026004', 'CS102', '2023B', 93.00),
    ('S2026004', 'CS201', '2024A', 91.00),
    ('S2026004', 'CS202', '2024B', 90.00),
    ('S2026004', 'CS301', '2025A', 87.00),
    ('S2026004', 'CS303', '2025B', 84.00),
    ('S2026006', 'CS101', '2023A', 86.00),
    ('S2026006', 'CS102', '2023B', 84.00),
    ('S2026006', 'CS201', '2024A', 82.00),
    ('S2026006', 'CS202', '2024B', 78.00),
    ('S2026006', 'CS301', '2025A', 75.00),
    ('S2026006', 'CS302', '2025B', 88.00),
    ('S2026006', 'CS303', '2026A', 80.00),
    ('S2026007', 'CS101', '2025A', 70.00),
    ('S2026007', 'CS102', '2025B', 64.00),
    ('S2026007', 'CS201', '2026A', 58.00),
    ('S2026008', 'CS101', '2024A', 78.00),
    ('S2026008', 'CS102', '2024B', 75.00),
    ('S2026008', 'CS201', '2025A', 73.00),
    ('S2026008', 'CS202', '2025B', 69.00),
    ('S2026009', 'CS101', '2026A', 89.00),
    ('S2026010', 'CS101', '2023A', 82.00),
    ('S2026010', 'CS102', '2023B', 80.00),
    ('S2026010', 'CS201', '2024A', 77.00),
    ('S2026010', 'CS202', '2024B', 72.00),
    ('S2026010', 'CS302', '2025A', 85.00)
) AS grade_data(student_number, course_code, semester, grade)
JOIN students s ON s.student_number = grade_data.student_number
JOIN courses c ON c.code = grade_data.course_code
JOIN enrollments e
    ON e.student_id = s.id
    AND e.course_id = c.id
    AND e.semester = grade_data.semester;
