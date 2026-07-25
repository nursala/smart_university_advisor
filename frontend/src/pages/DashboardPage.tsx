import { useEffect, useState } from 'react'
import { Link } from 'react-router-dom'
import { useAuth } from '../auth/AuthContext'
import {
  buildSemesterPlan,
  getAcademicSummary,
  getAvailableCourses,
  getCourseRecommendations,
  getProfile,
} from '../services/studentApi'
import LoadingState from '../components/LoadingState'
import ErrorMessage from '../components/ErrorMessage'
import EmptyState from '../components/EmptyState'
import DifficultyBadge from '../components/DifficultyBadge'
import type {
  AcademicSummary,
  CourseRecommendation,
  PlannedCourse,
  StudentProfile,
} from '../types/student'
import type { DifficultyLevel } from '../types/course'

type OverviewState =
  | { status: 'loading' }
  | { status: 'error'; message: string }
  | {
      status: 'loaded'
      profile: StudentProfile
      summary: AcademicSummary
      availableCoursesCount: number
    }

export default function DashboardPage() {
  const { user } = useAuth()
  const studentId = user?.student_id ?? null

  const [overview, setOverview] = useState<OverviewState>({ status: 'loading' })

  useEffect(() => {
    if (studentId === null) return
    let ignore = false

    setOverview({ status: 'loading' })

    Promise.all([
      getProfile(studentId),
      getAcademicSummary(studentId),
      getAvailableCourses(studentId),
    ])
      .then(([profile, summary, availableCourses]) => {
        if (ignore) return
        setOverview({
          status: 'loaded',
          profile,
          summary,
          availableCoursesCount: availableCourses.length,
        })
      })
      .catch((error: unknown) => {
        if (ignore) return
        setOverview({
          status: 'error',
          message: error instanceof Error ? error.message : 'Unable to load your dashboard.',
        })
      })

    return () => {
      ignore = true
    }
  }, [studentId])

  if (studentId === null) {
    return (
      <EmptyState message="Your account isn't linked to a student profile. Contact your advisor if you believe this is a mistake." />
    )
  }

  return (
    <div className="page">
      <header className="page-header">
        <h1>Welcome back{overview.status === 'loaded' ? `, ${overview.profile.name}` : ''}</h1>
        <p>Here's where your academic planning stands today.</p>
      </header>

      {overview.status === 'loading' && <LoadingState label="Loading your dashboard..." />}

      {overview.status === 'error' && (
        <ErrorMessage
          message={overview.message}
          onRetry={() => setOverview({ status: 'loading' })}
        />
      )}

      {overview.status === 'loaded' && (
        <>
          <section className="stat-grid" aria-label="Academic overview">
            <StatCard
              label="Current GPA"
              value={
                overview.summary.current_gpa === null
                  ? 'Not available'
                  : overview.summary.current_gpa.toFixed(2)
              }
            />
            <StatCard label="Completed credits" value={overview.summary.completed_credits} />
            <StatCard label="Completed courses" value={overview.summary.completed_courses_count} />
            <StatCard label="Active courses" value={overview.summary.active_courses_count} />
            <StatCard label="Available courses" value={overview.availableCoursesCount} />
            <StatCard label="Weekly credit limit" value={overview.profile.max_weekly_credits} />
          </section>

          <section className="quick-links">
            <Link to="/chat" className="quick-link-card">
              <h3>Ask the AI Advisor</h3>
              <p>Get grounded answers about your courses and plan.</p>
            </Link>
            <Link to="/courses" className="quick-link-card">
              <h3>Browse courses</h3>
              <p>Explore the catalog and check prerequisites.</p>
            </Link>
            <Link to="/profile" className="quick-link-card">
              <h3>View full profile</h3>
              <p>See your academic summary in detail.</p>
            </Link>
          </section>

          <RecommendationsPanel studentId={studentId} />
          <SemesterPlanPanel
            studentId={studentId}
            defaultMaxCredits={overview.profile.max_weekly_credits}
          />
        </>
      )}
    </div>
  )
}

function StatCard({ label, value }: { label: string; value: string | number }) {
  return (
    <div className="stat-card">
      <p className="stat-label">{label}</p>
      <p className="stat-value">{value}</p>
    </div>
  )
}

type RecommendationsState =
  | { status: 'idle' }
  | { status: 'loading' }
  | { status: 'error'; message: string }
  | { status: 'loaded'; recommendations: CourseRecommendation[] }

function RecommendationsPanel({ studentId }: { studentId: number }) {
  const [preferredDifficulty, setPreferredDifficulty] = useState<DifficultyLevel | ''>('')
  const [state, setState] = useState<RecommendationsState>({ status: 'idle' })

  async function handleGetRecommendations() {
    setState({ status: 'loading' })
    try {
      const response = await getCourseRecommendations(studentId, {
        preferredDifficulty: preferredDifficulty || undefined,
      })
      setState({ status: 'loaded', recommendations: response.recommendations })
    } catch (error) {
      setState({
        status: 'error',
        message: error instanceof Error ? error.message : 'Unable to load recommendations.',
      })
    }
  }

  return (
    <section className="panel">
      <div className="panel-header">
        <h2>Course recommendations</h2>
        <div className="panel-controls">
          <label className="inline-field">
            Preferred difficulty
            <select
              value={preferredDifficulty}
              onChange={(event) =>
                setPreferredDifficulty(event.target.value as DifficultyLevel | '')
              }
            >
              <option value="">Any</option>
              <option value="easy">Easy</option>
              <option value="medium">Medium</option>
              <option value="hard">Hard</option>
            </select>
          </label>
          <button type="button" onClick={handleGetRecommendations} disabled={state.status === 'loading'}>
            {state.status === 'loading' ? 'Loading...' : 'Get recommendations'}
          </button>
        </div>
      </div>

      {state.status === 'error' && <ErrorMessage message={state.message} onRetry={handleGetRecommendations} />}

      {state.status === 'loaded' && state.recommendations.length === 0 && (
        <EmptyState message="No eligible courses to recommend right now." />
      )}

      {state.status === 'loaded' && state.recommendations.length > 0 && (
        <ul className="recommendation-list">
          {state.recommendations.map((course) => (
            <li key={course.id} className="recommendation-item">
              <div>
                <p className="recommendation-title">
                  {course.code} &middot; {course.name}
                </p>
                <p className="recommendation-reason">{course.reason}</p>
              </div>
              <div className="recommendation-meta">
                <DifficultyBadge level={course.difficulty_level} />
                <span>{course.credits} credits</span>
              </div>
            </li>
          ))}
        </ul>
      )}
    </section>
  )
}

type SemesterPlanState =
  | { status: 'idle' }
  | { status: 'loading' }
  | { status: 'error'; message: string }
  | {
      status: 'loaded'
      totalCredits: number
      estimatedWeeklyHours: number
      courses: PlannedCourse[]
    }

function SemesterPlanPanel({
  studentId,
  defaultMaxCredits,
}: {
  studentId: number
  defaultMaxCredits: number
}) {
  const [maxCredits, setMaxCredits] = useState(String(defaultMaxCredits))
  const [state, setState] = useState<SemesterPlanState>({ status: 'idle' })

  async function handleBuildPlan() {
    const parsedMaxCredits = Number(maxCredits)
    if (!Number.isInteger(parsedMaxCredits) || parsedMaxCredits <= 0) {
      setState({ status: 'error', message: 'Max credits must be a positive whole number.' })
      return
    }

    setState({ status: 'loading' })
    try {
      const response = await buildSemesterPlan(studentId, {
        maxCredits: parsedMaxCredits,
      })
      setState({
        status: 'loaded',
        totalCredits: response.total_credits,
        estimatedWeeklyHours: response.estimated_weekly_hours,
        courses: response.courses,
      })
    } catch (error) {
      setState({
        status: 'error',
        message: error instanceof Error ? error.message : 'Unable to build a semester plan.',
      })
    }
  }

  return (
    <section className="panel">
      <div className="panel-header">
        <h2>Semester plan builder</h2>
        <div className="panel-controls">
          <label className="inline-field">
            Max credits
            <input
              type="number"
              min={1}
              value={maxCredits}
              onChange={(event) => setMaxCredits(event.target.value)}
            />
          </label>
          <button type="button" onClick={handleBuildPlan} disabled={state.status === 'loading'}>
            {state.status === 'loading' ? 'Building...' : 'Build semester plan'}
          </button>
        </div>
      </div>

      {state.status === 'error' && <ErrorMessage message={state.message} onRetry={handleBuildPlan} />}

      {state.status === 'loaded' && state.courses.length === 0 && (
        <EmptyState message="No available courses fit within that credit limit." />
      )}

      {state.status === 'loaded' && state.courses.length > 0 && (
        <>
          <p className="plan-summary">
            {state.totalCredits} credits &middot; ~{state.estimatedWeeklyHours} estimated
            weekly hours
          </p>
          <ul className="recommendation-list">
            {state.courses.map((course) => (
              <li key={course.id} className="recommendation-item">
                <div>
                  <p className="recommendation-title">
                    {course.code} &middot; {course.name}
                  </p>
                  <p className="recommendation-reason">{course.reason}</p>
                </div>
                <div className="recommendation-meta">
                  <DifficultyBadge level={course.difficulty_level} />
                  <span>{course.credits} credits</span>
                </div>
              </li>
            ))}
          </ul>
        </>
      )}
    </section>
  )
}
