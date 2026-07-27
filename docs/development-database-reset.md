# Resetting the development database

PostgreSQL runs the schema and seed initialization scripts only when its data
volume is empty. An existing development volume can therefore retain older
semester values or miss constraints that are present in the current schema.

## Diagnose the current volume

From the repository root in PowerShell, run:

```powershell
$dbUser = if ($env:DB_USER) { $env:DB_USER } else { "advisor" }
$dbName = if ($env:DB_NAME) { $env:DB_NAME } else { "smart_university_advisor" }
Get-Content -Raw database/semester-diagnostics.sql |
  docker compose exec -T db psql -X -U $dbUser -d $dbName
```

The diagnostic is read-only. It lists noncanonical enrollment semesters,
groups their counts by value, reports their total, and checks whether
`chk_enrollments_semester_format` exists.

## Apply the current schema to a fresh volume

> **Warning:** `docker compose down --volumes` permanently deletes the local
> development database volume and all data stored in it. Back up any local data
> that must be retained before running these commands.

From the repository root in PowerShell, run:

```powershell
docker compose down --volumes
docker compose up --build -d

$dbUser = if ($env:DB_USER) { $env:DB_USER } else { "advisor" }
$dbName = if ($env:DB_NAME) { $env:DB_NAME } else { "smart_university_advisor" }
docker compose exec db pg_isready -U $dbUser -d $dbName
Get-Content -Raw database/semester-diagnostics.sql |
  docker compose exec -T db psql -X -U $dbUser -d $dbName
```

Wait for `pg_isready` to report that the server is accepting connections. The
final diagnostic should report zero incompatible rows and confirm that the
semester CHECK constraint exists.

No automatic legacy-semester migration is provided. Values such as `2023A` and
`2023B` require an explicit, team-approved mapping before any data migration is
performed.
