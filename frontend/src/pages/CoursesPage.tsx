import { useEffect, useState } from 'react'
import type { FormEvent } from 'react'
import { useAuth } from '../auth/AuthContext'
import { getCourseDetails, getCourses } from '../services/courseApi'
import { analyzeRisk } from '../services/studentApi'
import LoadingState from '../components/LoadingState'
import ErrorMessage from '../components/ErrorMessage'
import EmptyState from '../components/EmptyState'
import DifficultyBadge from '../components/DifficultyBadge'
import RiskBadge from '../components/RiskBadge'
import type { CourseDetails, CourseFilters, CourseSummary, DifficultyLevel } from '../types/course'
import type { RiskAnalysisResponse } from '../types/student'

type CoursesState =
  | { status: 'loading' }
  | { status: 'error'; message: string }
  | { status: 'loaded'; courses: CourseSummary[] }

type DetailsState =
  | { status: 'idle' }
  | { status: 'loading' }
  | { status: 'error'; message: string }
  | { status: 'loaded'; details: CourseDetails }

type RiskState =
  | { status: 'idle' }
  | { status: 'loading' }
  | { status: 'error'; message: string }
  | { status: 'loaded'; result: RiskAnalysisResponse }

const emptyFilters: CourseFilters = { department: '', difficulty: '', credits: '', instructor: '' }

export default function CoursesPage() {
  const { user } = useAuth()
  const studentId = user?.student_id ?? null

  const [filters, setFilters] = useState<CourseFilters>(emptyFilters)
  const [appliedFilters, setAppliedFilters] = useState<CourseFilters>(emptyFilters)
  const [coursesState, setCoursesState] = useState<CoursesState>({ status: 'loading' })

  const [selectedCourseId, setSelectedCourseId] = useState<number | null>(null)
  const [detailsState, setDetailsState] = useState<DetailsState>({ status: 'idle' })

  const [selectedForRisk, setSelectedForRisk] = useState<Set<number>>(new Set())
  const [riskState, setRiskState] = useState<RiskState>({ status: 'idle' })

  useEffect(() => {
    let ignore = false
    setCoursesState({ status: 'loading' })

    getCourses(appliedFilters)
      .then((courses) => {
        if (!ignore) setCoursesState({ status: 'loaded', courses })
      })
      .catch((error: unknown) => {
        if (ignore) return
        setCoursesState({
          status: 'error',
          message: error instanceof Error ? error.message : 'Unable to load courses.',
        })
      })

    return () => {
      ignore = true
    }
  }, [appliedFilters])

  function handleApplyFilters(event: FormEvent<HTMLFormElement>) {
    event.preventDefault()
    setAppliedFilters(filters)
  }

  function handleClearFilters() {
    setFilters(emptyFilters)
    setAppliedFilters(emptyFilters)
  }

  function handleViewDetails(courseId: number) {
    setSelectedCourseId(courseId)
    setDetailsState({ status: 'loading' })
    getCourseDetails(courseId)
      .then((details) => setDetailsState({ status: 'loaded', details }))
      .catch((error: unknown) => {
        setDetailsState({
          status: 'error',
          message: error instanceof Error ? error.message : 'Unable to load course details.',
        })
      })
  }

  function toggleRiskSelection(courseId: number) {
    setSelectedForRisk((current) => {
      const next = new Set(current)
      if (next.has(courseId)) next.delete(courseId)
      else next.add(courseId)
      return next
    })
  }

  async function handleAnalyzeRisk() {
    if (studentId === null || selectedForRisk.size === 0) return

    setRiskState({ status: 'loading' })
    try {
      const result = await analyzeRisk(studentId, Array.from(selectedForRisk))
      setRiskState({ status: 'loaded', result })
    } catch (error) {
      setRiskState({
        status: 'error',
        message: error instanceof Error ? error.message : 'Unable to analyze risk.',
      })
    }
  }

  return (
    <div className="page">
      <header className="page-header">
        <h1>Courses</h1>
        <p>Browse the catalog, inspect prerequisites, and check workload risk.</p>
      </header>

      <form className="filter-bar" onSubmit={handleApplyFilters}>
        <label className="inline-field">
          Department
          <input
            value={filters.department}
            onChange={(event) => setFilters({ ...filters, department: event.target.value })}
            placeholder="e.g. Computer Science"
          />
        </label>

        <label className="inline-field">
          Difficulty
          <select
            value={filters.difficulty}
            onChange={(event) =>
              setFilters({ ...filters, difficulty: event.target.value as DifficultyLevel | '' })
            }
          >
            <option value="">Any</option>
            <option value="easy">Easy</option>
            <option value="medium">Medium</option>
            <option value="hard">Hard</option>
          </select>
        </label>

        <label className="inline-field">
          Credits
          <input
            type="number"
            min={0}
            value={filters.credits}
            onChange={(event) => setFilters({ ...filters, credits: event.target.value })}
          />
        </label>

        <label className="inline-field">
          Instructor
          <input
            value={filters.instructor}
            onChange={(event) => setFilters({ ...filters, instructor: event.target.value })}
            placeholder="Name"
          />
        </label>

        <div className="filter-actions">
          <button type="submit">Apply filters</button>
          <button type="button" className="secondary-button" onClick={handleClearFilters}>
            Clear
          </button>
        </div>
      </form>

      <div className="courses-layout">
        <div className="courses-list">
          {coursesState.status === 'loading' && <LoadingState label="Loading courses..." />}

          {coursesState.status === 'error' && (
            <ErrorMessage
              message={coursesState.message}
              onRetry={() => setAppliedFilters({ ...appliedFilters })}
            />
          )}

          {coursesState.status === 'loaded' && coursesState.courses.length === 0 && (
            <EmptyState message="No courses match these filters." />
          )}

          {coursesState.status === 'loaded' &&
            coursesState.courses.map((course) => (
              <article
                key={course.id}
                className={`course-card ${selectedCourseId === course.id ? 'course-card-selected' : ''}`}
              >
                <label className="course-card-select">
                  <input
                    type="checkbox"
                    checked={selectedForRisk.has(course.id)}
                    onChange={() => toggleRiskSelection(course.id)}
                    aria-label={`Select ${course.code} for risk analysis`}
                  />
                </label>

                <div className="course-card-body">
                  <p className="course-card-title">
                    {course.code} &middot; {course.name}
                  </p>
                  <p className="course-card-meta">
                    {course.department} &middot; {course.credits} credits
                  </p>
                  <DifficultyBadge level={course.difficulty_level} />
                </div>

                <button
                  type="button"
                  className="secondary-button"
                  onClick={() => handleViewDetails(course.id)}
                >
                  View details
                </button>
              </article>
            ))}
        </div>

        <aside className="courses-sidebar">
          <section className="panel">
            <h2>Course details</h2>
            {detailsState.status === 'idle' && (
              <EmptyState message="Select a course to see its full details and prerequisites." />
            )}
            {detailsState.status === 'loading' && <LoadingState label="Loading details..." />}
            {detailsState.status === 'error' && (
              <ErrorMessage
                message={detailsState.message}
                onRetry={() => selectedCourseId !== null && handleViewDetails(selectedCourseId)}
              />
            )}
            {detailsState.status === 'loaded' && (
              <div className="course-details">
                <p className="course-card-title">
                  {detailsState.details.code} &middot; {detailsState.details.name}
                </p>
                <p>{detailsState.details.description ?? 'No description available.'}</p>
                <dl className="detail-list">
                  <div>
                    <dt>Department</dt>
                    <dd>{detailsState.details.department}</dd>
                  </div>
                  <div>
                    <dt>Credits</dt>
                    <dd>{detailsState.details.credits}</dd>
                  </div>
                  <div>
                    <dt>Weekly hours</dt>
                    <dd>{detailsState.details.estimated_weekly_hours}</dd>
                  </div>
                  <div>
                    <dt>Instructor</dt>
                    <dd>
                      {detailsState.details.instructor_name ?? 'Unassigned'}
                    </dd>
                  </div>
                </dl>

                <h3>Prerequisites</h3>
                {detailsState.details.prerequisites.length === 0 ? (
                  <p className="muted-text">None</p>
                ) : (
                  <ul className="prerequisite-list">
                    {detailsState.details.prerequisites.map((prerequisite) => (
                      <li key={prerequisite.code}>
                        {prerequisite.code} &middot; {prerequisite.name} (min grade{' '}
                        {prerequisite.minimum_grade})
                      </li>
                    ))}
                  </ul>
                )}
              </div>
            )}
          </section>

          <section className="panel">
            <h2>Workload risk analysis</h2>
            <p className="muted-text">
              {selectedForRisk.size === 0
                ? 'Select courses from the list to analyze their combined workload risk.'
                : `${selectedForRisk.size} course${selectedForRisk.size === 1 ? '' : 's'} selected.`}
            </p>

            {studentId === null ? (
              <EmptyState message="Risk analysis requires a linked student profile." />
            ) : (
              <button
                type="button"
                onClick={handleAnalyzeRisk}
                disabled={selectedForRisk.size === 0 || riskState.status === 'loading'}
              >
                {riskState.status === 'loading' ? 'Analyzing...' : 'Analyze selected courses'}
              </button>
            )}

            {riskState.status === 'error' && (
              <ErrorMessage message={riskState.message} onRetry={handleAnalyzeRisk} />
            )}

            {riskState.status === 'loaded' && (
              <div className="risk-result">
                <RiskBadge level={riskState.result.risk_level} />
                <p className="plan-summary">
                  {riskState.result.total_credits} credits &middot; ~
                  {riskState.result.estimated_weekly_hours} hrs/week &middot;{' '}
                  {riskState.result.hard_courses_count} hard course
                  {riskState.result.hard_courses_count === 1 ? '' : 's'}
                </p>
                <ul className="prerequisite-list">
                  {riskState.result.reasons.map((reason) => (
                    <li key={reason}>{reason}</li>
                  ))}
                </ul>
                {riskState.result.recommendations.length > 0 && (
                  <>
                    <h3>Recommendations</h3>
                    <ul className="prerequisite-list">
                      {riskState.result.recommendations.map((recommendation) => (
                        <li key={recommendation}>{recommendation}</li>
                      ))}
                    </ul>
                  </>
                )}
              </div>
            )}
          </section>
        </aside>
      </div>
    </div>
  )
}
