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
            # Startup connection failures are expected until Drogon is ready.
        }
        Start-Sleep -Seconds 1
    }
    throw "Temporary Drogon API did not become ready within 90 seconds"
}

function Login-Student {
    param(
        [System.Net.Http.HttpClient]$Client,
        [string]$Email
    )

    $response = Send-ApiRequest `
        -Client $Client -Method "POST" -Path "/auth/login" `
        -Body @{
            email = $Email
            password = "DemoStudent2026!"
        } `
        -Token $null
    Assert-True ($response.Status -eq 200) "seeded student login must return 200"
    Assert-True ($null -ne $response.Body.token) "login must return a JWT"
    Assert-True ($response.Body.user.role -eq "student") `
        "seeded login must return a student"
    Assert-True ([int64]$response.Body.user.student_id -gt 0) `
        "seeded student must have a linked student_id"
    return $response.Body
}

function Get-EligibleCourses {
    param(
        [System.Net.Http.HttpClient]$Client,
        [int64]$StudentId,
        [string]$Token
    )

    $response = Send-ApiRequest `
        -Client $Client -Method "GET" `
        -Path "/students/$StudentId/available-courses" `
        -Body $null -Token $Token
    Assert-True ($response.Status -eq 200) "eligible courses must return 200"
    return @($response.Body)
}

function Get-PlannedEnrollments {
    param(
        [System.Net.Http.HttpClient]$Client,
        [string]$Token
    )

    $response = Send-ApiRequest `
        -Client $Client -Method "GET" -Path "/enrollments/planned" `
        -Body $null -Token $Token
    Assert-True ($response.Status -eq 200) "My Plan must return 200"
    return @($response.Body)
}

function Wait-ForCourseEligibility {
    param(
        [System.Net.Http.HttpClient]$Client,
        [int64]$StudentId,
        [int64]$CourseId,
        [string]$Token
    )

    for ($attempt = 1; $attempt -le 20; $attempt++) {
        $eligible = Get-EligibleCourses `
            -Client $Client -StudentId $StudentId -Token $Token
        if (@($eligible | Where-Object {
            [int64]$_.id -eq $CourseId
        }).Count -eq 1) {
            return
        }
        Start-Sleep -Milliseconds 250
    }
    throw "Assertion failed: removed course must return to eligible courses"
}

Add-Type -AssemblyName System.Net.Http

$suffix = [Guid]::NewGuid().ToString("N").Substring(0, 12)
$dbContainer = "sua-my-plan-db-$suffix"
$apiContainer = "sua-my-plan-api-$suffix"
$networkName = "sua-my-plan-network-$suffix"
$volumeName = "sua-my-plan-data-$suffix"
$imageName = "sua-my-plan-integration:$suffix"
$dbPassword = [Guid]::NewGuid().ToString("N")
$jwtSecret = ([Guid]::NewGuid().ToString("N") + [Guid]::NewGuid().ToString("N"))
$client = $null
$testFailed = $false

try {
    Write-Host "Building isolated My Plan integration image..."
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
    $baseUri = "http://127.0.0.1:$($portMatch.Groups[1].Value)"
    $client = New-Object System.Net.Http.HttpClient
    $client.BaseAddress = New-Object System.Uri($baseUri)
    $client.Timeout = [TimeSpan]::FromSeconds(15)
    Wait-ForApi -Client $client

    $owner = Login-Student -Client $client -Email "adam@example.com"
    $other = Login-Student -Client $client -Email "sara@example.com"
    $ownerId = [int64]$owner.user.student_id
    $otherId = [int64]$other.user.student_id

    $eligibleBefore = Get-EligibleCourses `
        -Client $client -StudentId $ownerId -Token $owner.token
    Assert-True ($eligibleBefore.Count -gt 0) `
        "seeded demo student must have at least one eligible course"
    $courseId = [int64]$eligibleBefore[0].id
    Assert-True ($courseId -gt 0) "selected eligible course must have a valid id"
    $semester = "2028-Fall"

    $override = Send-ApiRequest `
        -Client $client -Method "POST" -Path "/enrollments" `
        -Body @{
            course_id = $courseId
            semester = $semester
            student_id = $otherId
        } `
        -Token $owner.token
    Assert-True ($override.Status -eq 403) `
        "conflicting student_id must be rejected"

    $create = Send-ApiRequest `
        -Client $client -Method "POST" -Path "/enrollments" `
        -Body @{
            course_id = $courseId
            semester = $semester
        } `
        -Token $owner.token
    Assert-True ($create.Status -eq 201) "planned enrollment creation must return 201"
    Assert-True ([int64]$create.Body.student_id -eq $ownerId) `
        "created enrollment must belong to the JWT-owned student"
    Assert-True ($create.Body.status -eq "planned") `
        "created enrollment status must be planned"
    $enrollmentId = [int64]$create.Body.id
    Assert-True ($enrollmentId -gt 0) "created enrollment must have a valid id"

    $databaseRow = Invoke-Docker @(
        "exec", $dbContainer,
        "psql", "-X", "-U", "advisor", "-d", "smart_university_advisor",
        "-At", "-c",
        "SELECT student_id || '|' || status FROM enrollments WHERE id=$enrollmentId"
    )
    Assert-True (($databaseRow -join "").Trim() -eq "$ownerId|planned") `
        "PostgreSQL row must be owned by the authenticated student and planned"

    $planAfterCreate = Get-PlannedEnrollments `
        -Client $client -Token $owner.token
    $createdMatches = @(
        $planAfterCreate | Where-Object {
            [int64]$_.id -eq $enrollmentId -and
            [int64]$_.course_id -eq $courseId
        }
    )
    Assert-True ($createdMatches.Count -eq 1) `
        "created enrollment must appear exactly once in My Plan"

    $eligibleAfterCreate = Get-EligibleCourses `
        -Client $client -StudentId $ownerId -Token $owner.token
    Assert-True (
        @($eligibleAfterCreate | Where-Object { [int64]$_.id -eq $courseId }).Count -eq 0
    ) "planned course must disappear from eligible courses"

    $duplicate = Send-ApiRequest `
        -Client $client -Method "POST" -Path "/enrollments" `
        -Body @{
            course_id = $courseId
            semester = $semester
        } `
        -Token $owner.token
    Assert-True ($duplicate.Status -eq 409) `
        "duplicate planned enrollment must be rejected with 409"

    $crossStudentDelete = Send-ApiRequest `
        -Client $client -Method "DELETE" `
        -Path "/enrollments/$enrollmentId" `
        -Body $null -Token $other.token
    Assert-True ($crossStudentDelete.Status -eq 403) `
        "a second student must not remove the owner's enrollment"

    $planAfterForbiddenDelete = Get-PlannedEnrollments `
        -Client $client -Token $owner.token
    Assert-True (
        @($planAfterForbiddenDelete | Where-Object {
            [int64]$_.id -eq $enrollmentId
        }).Count -eq 1
    ) "forbidden removal must leave the enrollment in My Plan"

    $remove = Send-ApiRequest `
        -Client $client -Method "DELETE" `
        -Path "/enrollments/$enrollmentId" `
        -Body $null -Token $owner.token
    Assert-True ($remove.Status -eq 200) `
        "owner must be able to remove a planned enrollment"

    $planAfterRemove = Get-PlannedEnrollments `
        -Client $client -Token $owner.token
    Assert-True (
        @($planAfterRemove | Where-Object {
            [int64]$_.id -eq $enrollmentId
        }).Count -eq 0
    ) "removed enrollment must disappear from My Plan"

    $statusOverride = Send-ApiRequest `
        -Client $client -Method "POST" -Path "/enrollments" `
        -Body @{
            course_id = $courseId
            semester = $semester
            status = "completed"
        } `
        -Token $owner.token
    Assert-True ($statusOverride.Status -eq 201) `
        "status override request must remain a valid planned-add request"
    Assert-True ($statusOverride.Body.status -eq "planned") `
        "student-supplied status must not create a non-planned enrollment"
    $statusEnrollmentId = [int64]$statusOverride.Body.id

    $statusDatabaseRow = Invoke-Docker @(
        "exec", $dbContainer,
        "psql", "-X", "-U", "advisor", "-d", "smart_university_advisor",
        "-At", "-c",
        "SELECT status FROM enrollments WHERE id=$statusEnrollmentId"
    )
    Assert-True (($statusDatabaseRow -join "").Trim() -eq "planned") `
        "PostgreSQL must store planned despite a student status override"

    $removeStatusTest = Send-ApiRequest `
        -Client $client -Method "DELETE" `
        -Path "/enrollments/$statusEnrollmentId" `
        -Body $null -Token $owner.token
    Assert-True ($removeStatusTest.Status -eq 200) `
        "status-override test enrollment cleanup must succeed"

    Wait-ForCourseEligibility `
        -Client $client -StudentId $ownerId -CourseId $courseId `
        -Token $owner.token

    Write-Host "My Plan PostgreSQL integration: PASS"
    Write-Host "Validated real login, eligibility, create, ownership, duplicate rejection,"
    Write-Host "cross-student removal denial, planned-only status, removal, and eligibility return."
}
catch {
    $testFailed = $true
    Write-Error "My Plan PostgreSQL integration failed: $($_.Exception.Message)"
}
finally {
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
