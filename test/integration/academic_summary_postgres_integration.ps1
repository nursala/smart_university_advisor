[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        throw "Assertion failed: $Message"
    }
}

function Invoke-Docker {
    param([string[]]$Arguments)
    $previousErrorPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $output = & docker @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    $ErrorActionPreference = $previousErrorPreference
    if ($exitCode -ne 0) {
        throw "Docker command failed: docker $($Arguments[0])"
    }
    return $output
}

function Invoke-DatabaseScalar {
    param([string]$ContainerName, [string]$Sql)
    $output = Invoke-Docker @(
        "exec", $ContainerName,
        "psql", "-X", "-U", "advisor", "-d", "smart_university_advisor",
        "-Atq", "-v", "ON_ERROR_STOP=1", "-c", $Sql
    )
    return ($output -join "").Trim()
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
            $json = ConvertTo-Json -InputObject $Body -Depth 20 -Compress
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
                $parsed = ConvertFrom-Json -InputObject $content
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
            # Startup connection failures are expected until Drogon is ready.
        }
        Start-Sleep -Seconds 1
    }
    throw "Temporary Drogon API did not become ready within 90 seconds"
}

function Wait-ForMockGemini {
    param([string]$ContainerName)
    for ($attempt = 1; $attempt -le 60; $attempt++) {
        & docker exec $ContainerName python -c `
            "import urllib.request; urllib.request.urlopen('http://127.0.0.1:8081/health').read()" `
            *> $null
        if ($LASTEXITCODE -eq 0) {
            return
        }
        Start-Sleep -Seconds 1
    }
    throw "Temporary Gemini mock did not become ready within 60 seconds"
}

Add-Type -AssemblyName System.Net.Http

$suffix = [Guid]::NewGuid().ToString("N").Substring(0, 12)
$dbContainer = "sua-summary-db-$suffix"
$mockContainer = "sua-summary-gemini-$suffix"
$apiContainer = "sua-summary-api-$suffix"
$networkName = "sua-summary-network-$suffix"
$volumeName = "sua-summary-data-$suffix"
$imageName = "sua-summary-integration:$suffix"
$dbPassword = [Guid]::NewGuid().ToString("N")
$jwtSecret = ([Guid]::NewGuid().ToString("N") + [Guid]::NewGuid().ToString("N"))
$studentPassword = ([Guid]::NewGuid().ToString("N") + "Aa1!")
$studentEmail = "summary-$suffix@example.test"
$client = $null
$testFailed = $false

$mockServer = @'
import json
from http.server import BaseHTTPRequestHandler, HTTPServer

class Handler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        pass

    def do_GET(self):
        self.send_response(200)
        self.end_headers()

    def do_POST(self):
        length = int(self.headers.get("content-length", "0"))
        body = json.loads(self.rfile.read(length))
        declarations = (
            body.get("tools", [{}])[0].get("functionDeclarations", [])
            if body.get("tools") else []
        )
        responses = [
            part["functionResponse"]["response"]
            for turn in body.get("contents", [])
            for part in turn.get("parts", [])
            if isinstance(part, dict) and "functionResponse" in part
        ]

        if not responses:
            if len(declarations) != 8:
                self.send_response(500)
                self.end_headers()
                return
            payload = {
                "candidates": [{
                    "content": {
                        "parts": [{
                            "functionCall": {
                                "name": "get_academic_summary",
                                "args": {"student_id": 999999}
                            }
                        }]
                    }
                }]
            }
        else:
            data = responses[-1].get("data", {})
            planned = data.get("planned_courses", [])
            valid = (
                len(planned) == 1
                and planned[0].get("course_code") == "CS101"
                and planned[0].get("semester") == "2026-Fall"
                and planned[0].get("status") == "planned"
                and data.get("active_courses") == []
                and data.get("completed_courses") == []
            )
            text = (
                "You have CS101 — Introduction to Computer Science planned "
                "for 2026-Fall. You currently have no active or completed courses."
                if valid else "INVALID ACADEMIC SUMMARY"
            )
            payload = {
                "candidates": [{
                    "content": {"parts": [{"text": text}]}
                }]
            }

        encoded = json.dumps(payload).encode()
        self.send_response(200)
        self.send_header("content-type", "application/json")
        self.send_header("content-length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)

HTTPServer(("0.0.0.0", 8081), Handler).serve_forever()
'@
$mockServerEncoded = [Convert]::ToBase64String(
    [System.Text.Encoding]::UTF8.GetBytes($mockServer)
)
$mockCommand =
    "import base64;exec(base64.b64decode('$mockServerEncoded'))"

try {
    Write-Host "Building isolated academic-summary integration image..."
    & docker build --quiet --tag $imageName . *> $null
    if ($LASTEXITCODE -ne 0) {
        throw "Backend integration image build failed"
    }

    Invoke-Docker @("network", "create", $networkName) *> $null
    Invoke-Docker @("volume", "create", $volumeName) *> $null
    $schemaPath = (Resolve-Path "database/schema.sql").Path
    $seedPath = (Resolve-Path "database/seed.sql").Path

    Invoke-Docker @(
        "run", "--detach", "--name", $dbContainer,
        "--network", $networkName, "--network-alias", "db",
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
        "run", "--detach", "--name", $mockContainer,
        "--network", $networkName, "--network-alias", "gemini-mock",
        "python:3.12-alpine", "python", "-c", $mockCommand
    ) *> $null
    Wait-ForMockGemini -ContainerName $mockContainer

    Invoke-Docker @(
        "run", "--detach", "--name", $apiContainer,
        "--network", $networkName, "--publish", "127.0.0.1::8080",
        "--env", "DB_HOST=db", "--env", "DB_PORT=5432",
        "--env", "DB_NAME=smart_university_advisor",
        "--env", "DB_USER=advisor", "--env", "DB_PASSWORD=$dbPassword",
        "--env", "JWT_SECRET=$jwtSecret",
        "--env", "GEMINI_API_KEY=integration-placeholder",
        "--env", "GEMINI_MODEL=integration-model",
        "--env", "GEMINI_API_HOST=http://gemini-mock:8081",
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
            name = "Summary Integration Student"
            email = $studentEmail
            password = $studentPassword
        } `
        -Token $null
    Assert-True ($registration.Status -eq 201) "registration must return 201"
    $studentId = [int64]$registration.Body.user.student_id
    $token = [string]$registration.Body.token

    $insertedEnrollment = Invoke-DatabaseScalar `
        -ContainerName $dbContainer `
        -Sql (
            "INSERT INTO enrollments(student_id,course_id,semester,status) " +
            "SELECT $studentId,id,'2026-Fall','planned' FROM courses " +
            "WHERE code='CS101' RETURNING id"
        )
    Assert-True ([int64]$insertedEnrollment -gt 0) `
        "temporary planned enrollment must be created"

    $summary = Send-ApiRequest `
        -Client $client -Method "GET" `
        -Path "/students/$studentId/academic-summary" `
        -Body $null -Token $token
    Assert-True ($summary.Status -eq 200) "academic summary must return 200"
    Assert-True (@($summary.Body.completed_courses).Count -eq 0) `
        "completed courses must be empty"
    Assert-True (@($summary.Body.active_courses).Count -eq 0) `
        "active courses must be empty"
    Assert-True (@($summary.Body.planned_courses).Count -eq 1) `
        "planned courses must contain exactly one record"
    $planned = @($summary.Body.planned_courses)[0]
    Assert-True ([int64]$planned.enrollment_id -eq [int64]$insertedEnrollment) `
        "planned enrollment_id must match PostgreSQL"
    Assert-True ([int64]$planned.course_id -gt 0) "course_id must be present"
    Assert-True ($planned.course_code -eq "CS101") "course code must be CS101"
    Assert-True ($planned.course_name -eq "Introduction to Computer Science") `
        "course name must be correct"
    Assert-True ($planned.semester -eq "2026-Fall") "semester must be correct"
    Assert-True ([int]$planned.credits -eq 4) "credits must be correct"
    Assert-True ($planned.status -eq "planned") "status must be planned"

    $beforeAgent = Invoke-DatabaseScalar `
        -ContainerName $dbContainer `
        -Sql (
            "SELECT count(*) || '|' || md5(string_agg(" +
            "id::text || ':' || student_id::text || ':' || course_id::text || " +
            "':' || semester || ':' || status, ',' ORDER BY id)) " +
            "FROM enrollments"
        )
    $agent = Send-ApiRequest `
        -Client $client -Method "POST" -Path "/agent/query" `
        -Body @{ message = "What courses do I have?" } -Token $token
    if ($agent.Status -ne 200) {
        $agentError = if (
            $null -ne $agent.Body -and
            $agent.Body.PSObject.Properties.Name -contains "error"
        ) {
            [string]$agent.Body.error
        }
        else {
            "no structured error"
        }
        throw "Assertion failed: mocked Agent request returned HTTP $($agent.Status): $agentError"
    }
    Assert-True ($agent.Body.answer -match "CS101") `
        "Agent answer must mention CS101"
    Assert-True ($agent.Body.answer -match "2026-Fall") `
        "Agent answer must mention 2026-Fall"
    Assert-True ($agent.Body.answer -match "planned") `
        "Agent answer must describe CS101 as planned"
    Assert-True ($agent.Body.answer -notmatch "INVALID") `
        "Agent must receive the JWT-owned planned enrollment"
    Assert-True (@($agent.Body.tools_used).Count -eq 1) `
        "Agent must use exactly one tool"
    Assert-True (@($agent.Body.tools_used)[0] -eq "get_academic_summary") `
        "Agent must use get_academic_summary"

    $afterAgent = Invoke-DatabaseScalar `
        -ContainerName $dbContainer `
        -Sql (
            "SELECT count(*) || '|' || md5(string_agg(" +
            "id::text || ':' || student_id::text || ':' || course_id::text || " +
            "':' || semester || ':' || status, ',' ORDER BY id)) " +
            "FROM enrollments"
        )
    Assert-True ($afterAgent -eq $beforeAgent) `
        "read-only Agent request must not change enrollment rows"

    Write-Host "Academic-summary PostgreSQL integration: PASS"
    Write-Host "Validated separated course states, JWT-bound planned data,"
    Write-Host "correct mocked Agent synthesis, eight tools, and zero mutation."
}
catch {
    $testFailed = $true
    Write-Host `
        "Academic-summary integration failed: $($_.Exception.Message)" `
        -ForegroundColor Red
}
finally {
    if ($null -ne $client) {
        $client.Dispose()
    }
    $previousErrorPreference = $ErrorActionPreference
    $ErrorActionPreference = "SilentlyContinue"
    foreach ($container in @($apiContainer, $mockContainer, $dbContainer)) {
        & docker container inspect $container *> $null
        if ($LASTEXITCODE -eq 0) {
            & docker rm --force --volumes $container *> $null
        }
    }
    & docker volume inspect $volumeName *> $null
    if ($LASTEXITCODE -eq 0) {
        & docker volume rm --force $volumeName *> $null
    }
    & docker network inspect $networkName *> $null
    if ($LASTEXITCODE -eq 0) {
        & docker network rm $networkName *> $null
    }
    & docker image inspect $imageName *> $null
    if ($LASTEXITCODE -eq 0) {
        & docker image rm --force $imageName *> $null
    }
    $ErrorActionPreference = $previousErrorPreference
}

if ($testFailed) {
    exit 1
}
