# Smart University Advisor AI (C++)

Initial Drogon and PostgreSQL backend skeleton for the Smart University
Advisor project.

## Run

```sh
docker compose up
```

The API listens on `http://localhost:8080`. The initial end-to-end endpoint is:

```sh
curl http://localhost:8080/courses
```

Optional database settings can be overridden with `DB_NAME`, `DB_USER`, and
`DB_PASSWORD`. Their defaults are `smart_university_advisor`, `advisor`, and
`advisor_password`.
