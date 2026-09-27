# Smart University Advisor — Frontend
**React · TypeScript · Vite**

The web client for [Smart University Advisor](../README.md). It provides a course catalog, registration/login, a student profile, semester planning, and AI advisor chat.

## Run the complete application
From the repository root:

```bash
docker compose up --build
```

Open **http://localhost:5173**. See the [root README](../README.md) for environment configuration and local demo credentials.

## Frontend development
From this directory:

```bash
npm ci
cp .env.example .env
npm run dev
```

The backend must also be running. `VITE_API_BASE_URL` controls the client API base URL; the example uses `/api`.

## Commands
| Command | Purpose |
| --- | --- |
| `npm run dev` | Vite development server |
| `npm run build` | TypeScript check and production build |
| `npm run lint` | Oxlint checks |
| `npm run preview` | Preview a production build |

## Code map
- `src/pages/`: catalog, authentication, profile, plan, and chat screens.
- `src/services/`: typed API calls.
- `src/auth/`: session state and route protection.
- `src/chat/`: chat state retained during route navigation.
- `src/components/`: shared layout and feedback components.

Authentication uses `sessionStorage` and is validated through the backend. Chat messages are held in React memory and reset on page refresh or logout.
