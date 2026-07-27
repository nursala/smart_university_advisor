import { type FormEvent, useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { useAuth } from '../auth/AuthContext'
import {
  createPlannedEnrollment,
  getPlannedEnrollments,
  removePlannedEnrollment,
} from '../services/enrollmentApi'
import { getAvailableCourses } from '../services/studentApi'
import type { AvailableCourse, PlannedEnrollment } from '../types/student'
import LoadingState from '../components/LoadingState'
import ErrorMessage from '../components/ErrorMessage'
import EmptyState from '../components/EmptyState'

type State =
  | { status: 'loading' }
  | { status: 'error'; message: string }
  | {
      status: 'loaded'
      enrollments: PlannedEnrollment[]
      eligibleCourses: AvailableCourse[]
    }

function isPlannedEnrollment(value: unknown): value is PlannedEnrollment {
  if (!value || typeof value !== 'object') return false
  const item = value as Partial<PlannedEnrollment>
  return Number.isInteger(item.id) &&
    Number.isInteger(item.course_id) &&
    typeof item.course_code === 'string' &&
    typeof item.course_name === 'string' &&
    Number.isFinite(item.credits) &&
    typeof item.semester === 'string' &&
    item.status === 'planned'
}

function isAvailableCourse(value: unknown): value is AvailableCourse {
  if (!value || typeof value !== 'object') return false
  const item = value as Partial<AvailableCourse>
  return Number.isInteger(item.id) &&
    typeof item.code === 'string' &&
    typeof item.name === 'string' &&
    Number.isFinite(item.credits) &&
    ['easy', 'medium', 'hard'].includes(item.difficulty_level ?? '')
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
  const requestSequence = useRef(0)

  const load = useCallback(async () => {
    if (user?.student_id == null) return
    const sequence = ++requestSequence.current
    setState({ status: 'loading' })
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
        setState({ status: 'loaded', enrollments, eligibleCourses })
      }
    } catch (error) {
      if (sequence === requestSequence.current) {
        setState({
          status: 'error',
          message: error instanceof Error ? error.message : 'Unable to load My Plan.',
        })
      }
    }
  }, [user?.student_id])

  useEffect(() => {
    void load()
    return () => {
      requestSequence.current += 1
    }
  }, [load])

  const groups = useMemo(() => {
    if (state.status !== 'loaded') return []
    const grouped = new Map<string, PlannedEnrollment[]>()
    state.enrollments.forEach((item) =>
      grouped.set(item.semester, [...(grouped.get(item.semester) ?? []), item]),
    )
    return Array.from(grouped).sort(([left], [right]) => left.localeCompare(right))
  }, [state])

  async function add(event: FormEvent<HTMLFormElement>) {
    event.preventDefault()
    const courseId = Number(selectedCourseId)
    if (!Number.isInteger(courseId) || !semester) return
    setIsAdding(true)
    setActionError('')
    setSuccess('')
    try {
      await createPlannedEnrollment(courseId, semester)
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
