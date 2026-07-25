// Minimal stand-in for Gemini's `generateContent` endpoint, used only to
// load-test /agent/query's per-request state isolation under real
// concurrency without needing a real GEMINI_API_KEY (none is configured
// in this environment -- see docs/concurrency-test.md). Point
// GeminiClient at this via GEMINI_API_HOST=http://host.docker.internal:<port>.
//
// Behavior: parses the student_id embedded in AgentController's prompt
// text ("...helping student_id N...") and echoes it straight back as the
// model's final answer (no functionCall parts), so each request completes
// in one round trip. A per-request random delay increases the chance that
// concurrent requests genuinely overlap in-flight.
const http = require('http');

const PORT = process.env.MOCK_GEMINI_PORT || 5050;
const MIN_DELAY_MS = Number(process.env.MOCK_GEMINI_MIN_DELAY_MS || 50);
const MAX_DELAY_MS = Number(process.env.MOCK_GEMINI_MAX_DELAY_MS || 250);

function randomDelay()
{
    return MIN_DELAY_MS + Math.random() * (MAX_DELAY_MS - MIN_DELAY_MS);
}

const server = http.createServer((req, res) => {
    if (req.method !== 'POST')
    {
        res.writeHead(404);
        res.end();
        return;
    }

    let body = '';
    req.on('data', (chunk) => { body += chunk; });
    req.on('end', () => {
        let studentId = 'unknown';
        let parsed;
        try
        {
            parsed = JSON.parse(body);
            const text = parsed?.contents?.[0]?.parts?.[0]?.text || '';
            const match = text.match(/helping student_id (\d+)/);
            if (match)
            {
                studentId = match[1];
            }
        }
        catch (error)
        {
            // fall through with studentId = 'unknown'
        }

        setTimeout(() => {
            const firstText = parsed?.contents?.[0]?.parts?.[0]?.text || '';
            const hasToolResponse = (parsed?.contents || []).some((turn) =>
                (turn?.parts || []).some((part) => part?.functionResponse));
            const toolResponses = (parsed?.contents || []).flatMap((turn) =>
                (turn?.parts || []).filter((part) => part?.functionResponse));
            let parts;
            if (firstText.includes('multi-tool-demo'))
            {
                const chain = [
                    { name: 'get_student_profile', args: { student_id: 999999 } },
                    { name: 'get_academic_summary', args: { student_id: 999999 } },
                    { name: 'get_available_courses', args: { student_id: 999999 } },
                    { name: 'build_semester_plan', args: { student_id: 999999, max_credits: 12 } },
                ];
                parts = toolResponses.length < chain.length
                    ? [{ functionCall: chain[toolResponses.length] }]
                    : [{ text: 'Your profile and academic summary were reviewed. Eligible courses were checked, and a balanced plan within 12 credits was prepared from those results.' }];
            }
            else
            {
                parts =
                firstText.includes('enroll-test') && !hasToolResponse
                    ? [{
                        functionCall: {
                            name: 'enroll_in_course',
                            args: {
                                student_id: 999999,
                                course_id: 1,
                                semester: '2028-Fall',
                            },
                        },
                    }]
                    : [{ text: `Echo: ${studentId}` }];
            }
            const response = {
                candidates: [
                    {
                        content: {
                            parts,
                        },
                    },
                ],
            };
            res.writeHead(200, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify(response));
        }, randomDelay());
    });
});

server.listen(PORT, () => {
    console.log(`Mock Gemini server listening on port ${PORT}`);
});
