[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw "Assertion failed: $Message"
    }
}

function Invoke-Docker {
    param([string[]]$Arguments)

    $output = & docker @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Docker command failed: docker $($Arguments[0])"
    }
    return $output
}

function Invoke-DatabaseScalar {
    param(
        [string]$ContainerName,
        [string]$Sql
    )

    $output = Invoke-Docker @(
        "exec", $ContainerName,
        "psql", "-X", "-U", "advisor", "-d", "smart_university_advisor",
        "-At", "-v", "ON_ERROR_STOP=1", "-c", $Sql
    )
    return ($output -join "").Trim()
}

function Invoke-DatabaseScript {
    param(
        [string]$ContainerName,
        [string]$Sql
    )

    $Sql | & docker exec -i $ContainerName `
        psql -X -U advisor -d smart_university_advisor `
        -v ON_ERROR_STOP=1 *> $null
    if ($LASTEXITCODE -ne 0) {
        throw "Temporary PostgreSQL setup command failed"
    }
}

function Send-ApiRequest {
    param(
        [System.Net.Http.HttpClient]$Client,
        [string]$Method,
        [string]$Path,
        [AllowNull()][object]$Body,
        [AllowNull()][string]$Token
    )

    $request = New-Object System.Net.Http.HttpRequestMessage(
        (New-Object System.Net.Http.HttpMethod($Method)),
        $Path
    )
    try {
        if ($Token) {
            $request.Headers.Authorization =
                New-Object System.Net.Http.Headers.AuthenticationHeaderValue(
                    "Bearer",
                    $Token
                )
        }
        if ($null -ne $Body) {
            $json = ConvertTo-Json -InputObject $Body -Depth 10 -Compress
            $request.Content = New-Object System.Net.Http.StringContent(
                $json,
                [System.Text.Encoding]::UTF8,
                "application/json"
            )
        }

        $response = $Client.SendAsync($request).GetAwaiter().GetResult()
        try {
            $content = $response.Content.ReadAsStringAsync().GetAwaiter().GetResult()
            $parsed = $null
            if (-not [string]::IsNullOrWhiteSpace($content)) {
                try {
                    $parsed = ConvertFrom-Json -InputObject $content
                }
                catch {
                    throw "API returned malformed JSON for $Method $Path"
                }
            }
            return [PSCustomObject]@{
                Status = [int]$response.StatusCode
                Body = $parsed
            }
        }
        finally {
            $response.Dispose()
        }
    }
    finally {
        $request.Dispose()
    }
}

function Wait-ForPostgres {
    param([string]$ContainerName)

    for ($attempt = 1; $attempt -le 60; $attempt++) {
        & docker exec $ContainerName pg_isready `
            -U advisor -d smart_university_advisor *> $null
        if ($LASTEXITCODE -eq 0) {
            return
        }
        Start-Sleep -Seconds 1
    }
    throw "Temporary PostgreSQL did not become ready within 60 seconds"
}

function Wait-ForApi {
    param([System.Net.Http.HttpClient]$Client)

    for ($attempt = 1; $attempt -le 90; $attempt++) {
        try {
            $response = Send-ApiRequest `
                -Client $Client -Method "GET" -Path "/courses" `
                -Body $null -Token $null
            if ($response.Status -eq 200) {
                return
            }
        }
        catch {
            # Connection failures are expected while Drogon starts.
        }
        Start-Sleep -Seconds 1
    }
    throw "Temporary Drogon API did not become ready within 90 seconds"
}

Add-Type -AssemblyName System.Net.Http

$suffix = [Guid]::NewGuid().ToString("N").Substring(0, 12)
$dbContainer = "sua-registration-db-$suffix"
$apiContainer = "sua-registration-api-$suffix"
$networkName = "sua-registration-network-$suffix"
$volumeName = "sua-registration-data-$suffix"
$imageName = "sua-registration-integration:$suffix"
$dbPassword = [Guid]::NewGuid().ToString("N")
$jwtSecret = ([Guid]::NewGuid().ToString("N") + [Guid]::NewGuid().ToString("N"))
$registrationPassword =
    ([Guid]::NewGuid().ToString("N") + "Aa1!")
$registeredEmail = "registration-$suffix@example.test"
$rollbackEmail = "rollback-$suffix@example.test"
$registeredName = "Integration Student"
$client = $null
$testFailed = $false
$failureTriggerInstalled = $false

try {
    Write-Host "Building isolated registration integration image..."
    & docker build --quiet --tag $imageName . *> $null
    if ($LASTEXITCODE -ne 0) {
        throw "Backend integration image build failed"
    }

    Invoke-Docker @("network", "create", $networkName) *> $null
    Invoke-Docker @("volume", "create", $volumeName) *> $null

    $schemaPath = (Resolve-Path "database/schema.sql").Path
    $seedPath = (Resolve-Path "database/seed.sql").Path
    Invoke-Docker @(
        "run", "--detach",
        "--name", $dbContainer,
        "--network", $networkName,
        "--network-alias", "db",
        "--env", "POSTGRES_DB=smart_university_advisor",
        "--env", "POSTGRES_USER=advisor",
        "--env", "POSTGRES_PASSWORD=$dbPassword",
        "--volume", "${volumeName}:/var/lib/postgresql/data",
        "--volume", "${schemaPath}:/docker-entrypoint-initdb.d/01-schema.sql:ro",
        "--volume", "${seedPath}:/docker-entrypoint-initdb.d/02-seed.sql:ro",
        "postgres:16"
    ) *> $null
    Wait-ForPostgres -ContainerName $dbContainer

    Invoke-Docker @(
        "run", "--detach",
        "--name", $apiContainer,
        "--network", $networkName,
        "--publish", "127.0.0.1::8080",
        "--env", "DB_HOST=db",
        "--env", "DB_PORT=5432",
        "--env", "DB_NAME=smart_university_advisor",
        "--env", "DB_USER=advisor",
        "--env", "DB_PASSWORD=$dbPassword",
        "--env", "JWT_SECRET=$jwtSecret",
        "--env", "GEMINI_API_KEY=",
        $imageName
    ) *> $null

    $portOutput = Invoke-Docker @("port", $apiContainer, "8080/tcp")
    $portMatch = [regex]::Match(($portOutput -join "`n"), "127\.0\.0\.1:(\d+)")
    Assert-True $portMatch.Success "temporary API port must be discoverable"
    $client = New-Object System.Net.Http.HttpClient
    $client.BaseAddress = New-Object System.Uri(
        "http://127.0.0.1:$($portMatch.Groups[1].Value)"
    )
    $client.Timeout = [TimeSpan]::FromSeconds(15)
    Wait-ForApi -Client $client

    $registration = Send-ApiRequest `
        -Client $client -Method "POST" -Path "/auth/register" `
        -Body @{
            name = $registeredName
            email = $registeredEmail
            password = $registrationPassword
            role = "admin"
        } `
        -Token $null

    Assert-True ($registration.Status -eq 201) `
        "successful registration must return HTTP 201"
    Assert-True (-not [string]::IsNullOrWhiteSpace($registration.Body.token)) `
        "registration response must include a token"
    Assert-True ([int64]$registration.Body.user.id -gt 0) `
        "registration response must include user.id"
    Assert-True ($registration.Body.user.name -eq $registeredName) `
        "registration response must include the submitted name"
    Assert-True ($registration.Body.user.email -eq $registeredEmail) `
        "registration response must include the submitted email"
    Assert-True ($registration.Body.user.role -eq "student") `
        "privileged role injection must be ignored"
    Assert-True ($null -ne $registration.Body.user.student_id) `
        "registration response must include a non-null student_id"
    Assert-True ([int64]$registration.Body.user.student_id -gt 0) `
        "registration response student_id must be positive"

    $userId = [int64]$registration.Body.user.id
    $studentId = [int64]$registration.Body.user.student_id
    $userCount = Invoke-DatabaseScalar `
        -ContainerName $dbContainer `
        -Sql "SELECT count(*) FROM users WHERE email='$registeredEmail'"
    Assert-True ($userCount -eq "1") `
        "successful registration must create exactly one users row"

    $studentCount = Invoke-DatabaseScalar `
        -ContainerName $dbContainer `
        -Sql (
            "SELECT count(*) FROM students s JOIN users u ON u.id=s.user_id " +
            "WHERE u.email='$registeredEmail'"
        )
    Assert-True ($studentCount -eq "1") `
        "successful registration must create exactly one linked students row"

    $databaseLink = Invoke-DatabaseScalar `
        -ContainerName $dbContainer `
        -Sql (
            "SELECT u.id || '|' || u.role || '|' || s.id || '|' || s.user_id " +
            "FROM users u JOIN students s ON s.user_id=u.id " +
            "WHERE u.email='$registeredEmail'"
        )
    Assert-True ($databaseLink -eq "$userId|student|$studentId|$userId") `
        "student row must reference the returned student user"

    $login = Send-ApiRequest `
        -Client $client -Method "POST" -Path "/auth/login" `
        -Body @{
            email = $registeredEmail
            password = $registrationPassword
        } `
        -Token $null
    Assert-True ($login.Status -eq 200) `
        "newly registered credentials must log in"
    Assert-True (-not [string]::IsNullOrWhiteSpace($login.Body.token)) `
        "login must return a token"
    Assert-True ([int64]$login.Body.user.id -eq $userId) `
        "login must return the registered user"
    Assert-True ([int64]$login.Body.user.student_id -eq $studentId) `
        "login must return the linked student_id"

    $me = Send-ApiRequest `
        -Client $client -Method "GET" -Path "/users/me" `
        -Body $null -Token $registration.Body.token
    Assert-True ($me.Status -eq 200) `
        "/users/me must accept the registration token"
    Assert-True ([int64]$me.Body.id -eq $userId) `
        "/users/me must return the registered user"
    Assert-True ([int64]$me.Body.student_id -eq $studentId) `
        "/users/me must return the linked student"
    Assert-True ($me.Body.role -eq "student") `
        "/users/me must preserve the student role"

    $duplicate = Send-ApiRequest `
        -Client $client -Method "POST" -Path "/auth/register" `
        -Body @{
            name = "Duplicate Student"
            email = $registeredEmail
            password = $registrationPassword
        } `
        -Token $null
    Assert-True ($duplicate.Status -eq 400) `
        "duplicate registration must be rejected with HTTP 400"

    $duplicateUserCount = Invoke-DatabaseScalar `
        -ContainerName $dbContainer `
        -Sql "SELECT count(*) FROM users WHERE email='$registeredEmail'"
    Assert-True ($duplicateUserCount -eq "1") `
        "duplicate registration must not create a second users row"
    $duplicateStudentCount = Invoke-DatabaseScalar `
        -ContainerName $dbContainer `
        -Sql (
            "SELECT count(*) FROM students s JOIN users u ON u.id=s.user_id " +
            "WHERE u.email='$registeredEmail'"
        )
    Assert-True ($duplicateStudentCount -eq "1") `
        "duplicate registration must not create a second students row"

    $installFailureTrigger = @'
CREATE FUNCTION integration_reject_student_insert()
RETURNS trigger
LANGUAGE plpgsql
AS $$
BEGIN
    RAISE EXCEPTION 'controlled integration failure';
END;
$$;

CREATE TRIGGER integration_reject_student_insert
BEFORE INSERT ON students
FOR EACH ROW
EXECUTE FUNCTION integration_reject_student_insert();
'@
    Invoke-DatabaseScript `
        -ContainerName $dbContainer -Sql $installFailureTrigger
    $failureTriggerInstalled = $true

    $rollbackRegistration = Send-ApiRequest `
        -Client $client -Method "POST" -Path "/auth/register" `
        -Body @{
            name = "Rollback Student"
            email = $rollbackEmail
            password = $registrationPassword
        } `
        -Token $null
    Assert-True ($rollbackRegistration.Status -eq 500) `
        "controlled linked-student failure must return HTTP 500"

    $rollbackUserCount = Invoke-DatabaseScalar `
        -ContainerName $dbContainer `
        -Sql "SELECT count(*) FROM users WHERE email='$rollbackEmail'"
    Assert-True ($rollbackUserCount -eq "0") `
        "failed linked-student insertion must roll back the users row"
    $rollbackStudentCount = Invoke-DatabaseScalar `
        -ContainerName $dbContainer `
        -Sql (
            "SELECT count(*) FROM students s JOIN users u ON u.id=s.user_id " +
            "WHERE u.email='$rollbackEmail'"
        )
    Assert-True ($rollbackStudentCount -eq "0") `
        "failed registration must leave no students row"

    Invoke-DatabaseScript -ContainerName $dbContainer -Sql @'
DROP TRIGGER integration_reject_student_insert ON students;
DROP FUNCTION integration_reject_student_insert();
'@
    $failureTriggerInstalled = $false

    $triggerCount = Invoke-DatabaseScalar `
        -ContainerName $dbContainer `
        -Sql (
            "SELECT count(*) FROM pg_trigger " +
            "WHERE tgname='integration_reject_student_insert' AND NOT tgisinternal"
        )
    Assert-True ($triggerCount -eq "0") `
        "controlled failure trigger must be removed"

    Write-Host "Registration PostgreSQL integration: PASS"
    Write-Host "Validated student-only registration, linked rows, login, /users/me,"
    Write-Host "duplicate rejection, and atomic rollback under controlled failure."
}
catch {
    $testFailed = $true
    Write-Error "Registration PostgreSQL integration failed: $($_.Exception.Message)"
}
finally {
    if ($failureTriggerInstalled) {
        try {
            Invoke-DatabaseScript -ContainerName $dbContainer -Sql @'
DROP TRIGGER IF EXISTS integration_reject_student_insert ON students;
DROP FUNCTION IF EXISTS integration_reject_student_insert();
'@
        }
        catch {
            # The disposable database is still removed below.
        }
    }
    if ($null -ne $client) {
        $client.Dispose()
    }

    & docker rm --force --volumes $apiContainer $dbContainer *> $null
    & docker volume rm --force $volumeName *> $null
    & docker network rm $networkName *> $null
    & docker image rm --force $imageName *> $null
}

if ($testFailed) {
    exit 1
}
