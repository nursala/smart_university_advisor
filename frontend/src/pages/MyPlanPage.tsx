import { type FormEvent, useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { useAuth } from '../auth/AuthContext'
import {
  createPlannedEnrollment,
  getPlannedEnrollments,
  removePlannedEnrollment,
} from '../services/enrollmentApi'
import { getAvailableCourses } from '../services/studentApi'
import type {
  EnrollmentCreationResponse,
  PlannedEnrollmentListItem,
} from '../types/enrollment'
import type { AvailableCourseListItem } from '../types/student'
import type { DifficultyLevel } from '../types/course'
import LoadingState from '../components/LoadingState'
import ErrorMessage from '../components/ErrorMessage'
import EmptyState from '../components/EmptyState'

type State =
  | { status: 'loading' }
  | { status: 'error'; message: string }
  | {
      status: 'loaded'
      enrollments: PlannedEnrollmentListItem[]
      eligibleCourses: AvailableCourseListItem[]
    }

const semesterTermRank = {
  Spring: 0,
  Summer: 1,
  Fall: 2,
  Winter: 3,
} as const

function compareSemesters(left: string, right: string) {
  const leftMatch = /^([0-9]{4})-(Spring|Summer|Fall|Winter)$/.exec(left)
  const rightMatch = /^([0-9]{4})-(Spring|Summer|Fall|Winter)$/.exec(right)
  if (!leftMatch || !rightMatch) return left.localeCompare(right)

  const yearDifference = Number(leftMatch[1]) - Number(rightMatch[1])
  if (yearDifference !== 0) return yearDifference

  const leftTerm = leftMatch[2] as keyof typeof semesterTermRank
  const rightTerm = rightMatch[2] as keyof typeof semesterTermRank
  return semesterTermRank[leftTerm] - semesterTermRank[rightTerm]
}

function isPositiveInteger(value: unknown): value is number {
  return Number.isInteger(value) && Number(value) > 0
}

function isNonEmptyString(value: unknown): value is string {
  return typeof value === 'string' && value.length > 0
}

function isDifficulty(value: unknown): value is DifficultyLevel {
  return value === 'easy' || value === 'medium' || value === 'hard'
}

function isCanonicalSemester(value: unknown): value is string {
  return typeof value === 'string' &&
    /^[0-9]{4}-(Spring|Summer|Fall|Winter)$/.test(value)
}

function isPlannedEnrollment(
  value: unknown,
): value is PlannedEnrollmentListItem {
  if (!value || typeof value !== 'object') return false
  const item = value as Partial<PlannedEnrollmentListItem>
  return isPositiveInteger(item.id) &&
    isPositiveInteger(item.student_id) &&
    isPositiveInteger(item.course_id) &&
    isNonEmptyString(item.course_code) &&
    isNonEmptyString(item.course_name) &&
    Number.isInteger(item.credits) &&
    Number(item.credits) > 0 &&
    isDifficulty(item.difficulty_level) &&
    isCanonicalSemester(item.semester) &&
    item.status === 'planned' &&
    isNonEmptyString(item.enrolled_at)
}

function isAvailableCourse(value: unknown): value is AvailableCourseListItem {
  if (!value || typeof value !== 'object') return false
  const item = value as Partial<AvailableCourseListItem>
  return isPositiveInteger(item.id) &&
    isNonEmptyString(item.code) &&
    isNonEmptyString(item.name) &&
    isNonEmptyString(item.department) &&
    Number.isInteger(item.credits) &&
    Number(item.credits) > 0 &&
    isDifficulty(item.difficulty_level)
}

function isEnrollmentCreationResponse(
  value: unknown,
): value is EnrollmentCreationResponse {
  if (!value || typeof value !== 'object') return false
  const item = value as Partial<EnrollmentCreationResponse>
  return isPositiveInteger(item.id) &&
    isPositiveInteger(item.student_id) &&
    isPositiveInteger(item.course_id) &&
    isCanonicalSemester(item.semester) &&
    item.status === 'planned' &&
    isNonEmptyString(item.enrolled_at)
}

export default function MyPlanPage() {
  const { user } = useAuth()
  const [state, setState] = useState<State>({ status: 'loading' })
  const [selectedCourseId, setSelectedCourseId] = useState('')
  const [semester, setSemester] = useState('')
  const [isAdding, setIsAdding] = useState(false)
  const [removingId, setRemovingId] = useState<number | null>(null)
  const [actionError, setActionError] = useState('')
  const [success, setSuccess] = useState('')
  const [refreshWarning, setRefreshWarning] = useState('')
  const requestSequence = useRef(0)
  const hasSuccessfullyLoaded = useRef(false)

  const load = useCallback(async () => {
    if (user?.student_id == null) return
    const sequence = ++requestSequence.current
    if (!hasSuccessfullyLoaded.current) {
      setState({ status: 'loading' })
    }
    try {
      const [enrollments, eligibleCourses] = await Promise.all([
        getPlannedEnrollments(),
        getAvailableCourses(user.student_id),
      ])
      if (!Array.isArray(enrollments) || !enrollments.every(isPlannedEnrollment)) {
        throw new Error('The server returned malformed planned-enrollment data.')
      }
      if (!Array.isArray(eligibleCourses) || !eligibleCourses.every(isAvailableCourse)) {
        throw new Error('The server returned malformed eligible-course data.')
      }
      if (sequence === requestSequence.current) {
        hasSuccessfullyLoaded.current = true
        setRefreshWarning('')
        setState({ status: 'loaded', enrollments, eligibleCourses })
      }
    } catch (error) {
      if (sequence === requestSequence.current) {
        const message = error instanceof Error ? error.message : 'Unable to load My Plan.'
        if (hasSuccessfullyLoaded.current) {
          setRefreshWarning(`Unable to refresh My Plan. ${message}`)
        } else {
          setState({ status: 'error', message })
        }
      }
    }
  }, [user?.student_id])

  useEffect(() => {
    hasSuccessfullyLoaded.current = false
    setRefreshWarning('')
    setState({ status: 'loading' })
    void load()
    return () => {
      requestSequence.current += 1
    }
  }, [load])

  const groups = useMemo(() => {
    if (state.status !== 'loaded') return []
    const grouped = new Map<string, PlannedEnrollmentListItem[]>()
    state.enrollments.forEach((item) =>
      grouped.set(item.semester, [...(grouped.get(item.semester) ?? []), item]),
    )
    return Array.from(grouped).sort(([left], [right]) => compareSemesters(left, right))
  }, [state])

  async function add(event: FormEvent<HTMLFormElement>) {
    event.preventDefault()
    const courseId = Number(selectedCourseId)
    if (!Number.isInteger(courseId) || !semester) return
    setIsAdding(true)
    setActionError('')
    setSuccess('')
    try {
      const created = await createPlannedEnrollment(courseId, semester)
      if (!isEnrollmentCreationResponse(created)) {
        throw new Error('The server returned malformed enrollment-creation data.')
      }
      setSelectedCourseId('')
      setSuccess('Course added to My Plan.')
      await load()
    } catch (error) {
      setActionError(error instanceof Error ? error.message : 'Unable to add course.')
    } finally {
      setIsAdding(false)
    }
  }

  async function remove(enrollmentId: number) {
    setRemovingId(enrollmentId)
    setActionError('')
    setSuccess('')
    try {
      await removePlannedEnrollment(enrollmentId)
      setSuccess('Course removed from My Plan.')
      await load()
    } catch (error) {
      setActionError(
        error instanceof Error ? error.message : 'Unable to remove enrollment.',
      )
    } finally {
      setRemovingId(null)
    }
  }

  if (user?.student_id == null) {
    return <EmptyState message="My Plan is available only to student accounts." />
  }

  return (
    <div className="page">
      <header className="page-header">
        <h1>My Plan</h1>
        <p>
          Add eligible courses manually. The server enforces prerequisites,
          duplicate prevention, semester format, and your credit limit.
        </p>
      </header>
      {state.status === 'loading' && <LoadingState label="Loading My Plan..." />}
      {state.status === 'error' && (
        <ErrorMessage message={state.message} onRetry={() => void load()} />
      )}
      {refreshWarning && state.status === 'loaded' && (
        <div className="state-panel state-warning" role="status">
          <p>{refreshWarning} Your last loaded plan is still displayed.</p>
          <button type="button" className="retry-button" onClick={() => void load()}>
            Try again
          </button>
        </div>
      )}
      {actionError && <ErrorMessage message={actionError} />}
      {success && <p className="success-message" role="status">{success}</p>}
      {state.status === 'loaded' && state.enrollments.length === 0 && (
        <EmptyState message="You have no planned courses yet. Select an eligible course and semester below to add it to your plan." />
      )}
      {groups.map(([groupSemester, enrollments]) => (
        <section className="panel" key={groupSemester}>
          <h2>{groupSemester}</h2>
          <p>{enrollments.reduce((total, item) => total + item.credits, 0)} planned credits</p>
          {enrollments.map((item) => (
            <article className="course-card" key={item.id}>
              <div className="course-card-body">
                <p className="course-card-title">{item.course_code} · {item.course_name}</p>
                <p className="course-card-meta">{item.credits} credits · {item.difficulty_level} · Planned</p>
              </div>
              <button
                type="button"
                className="secondary-button"
                onClick={() => void remove(item.id)}
                disabled={removingId !== null || isAdding}
              >
                {removingId === item.id ? 'Removing...' : 'Remove'}
              </button>
            </article>
          ))}
        </section>
      ))}
      {state.status === 'loaded' && (
        <section className="panel">
          <h2>Add to My Plan</h2>
          <form className="plan-form" onSubmit={add}>
            <div className="form-field">
              <label htmlFor="eligible-course">Eligible course</label>
              <select
                id="eligible-course"
                value={selectedCourseId}
                onChange={(event) => setSelectedCourseId(event.target.value)}
                disabled={isAdding || removingId !== null}
                required
              >
                <option value="">Select an eligible course</option>
                {state.eligibleCourses.map((course) => (
                  <option value={course.id} key={course.id}>
                    {course.code} · {course.name} — {course.credits} credits · {course.difficulty_level}
                  </option>
                ))}
              </select>
            </div>
            <div className="form-field">
              <label htmlFor="semester">Semester</label>
              <input
                id="semester"
                value={semester}
                onChange={(event) => setSemester(event.target.value)}
                placeholder="2026-Fall"
                pattern="[0-9]{4}-(Spring|Summer|Fall|Winter)"
                title="Use YYYY-Spring, YYYY-Summer, YYYY-Fall, or YYYY-Winter"
                disabled={isAdding || removingId !== null}
                required
              />
            </div>
            <div className="actions">
              <button
                type="submit"
                disabled={isAdding || removingId !== null || !selectedCourseId || !semester}
              >
                {isAdding ? 'Adding...' : 'Add to My Plan'}
              </button>
            </div>
          </form>
          {state.eligibleCourses.length === 0 && (
            <p>No eligible courses are currently available.</p>
          )}
        </section>
      )}
    </div>
  )
}
