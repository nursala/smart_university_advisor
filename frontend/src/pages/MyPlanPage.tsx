import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { useAuth } from '../auth/AuthContext'
import { getPlannedEnrollments, removePlannedEnrollment } from '../services/enrollmentApi'
import type { PlannedEnrollment } from '../types/student'
import LoadingState from '../components/LoadingState'
import ErrorMessage from '../components/ErrorMessage'
import EmptyState from '../components/EmptyState'

type State =
  | { status: 'loading' }
  | { status: 'error'; message: string }
  | { status: 'loaded'; enrollments: PlannedEnrollment[] }

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

export default function MyPlanPage() {
  const { user } = useAuth()
  const [state, setState] = useState<State>({ status: 'loading' })
  const [removingId, setRemovingId] = useState<number | null>(null)
  const [removalError, setRemovalError] = useState('')
  const requestSequence = useRef(0)

  const load = useCallback(async () => {
    if (user?.student_id == null) return
    const sequence = ++requestSequence.current
    setState({ status: 'loading' })
    setRemovalError('')
    try {
      const response = await getPlannedEnrollments()
      if (!Array.isArray(response) || !response.every(isPlannedEnrollment)) {
        throw new Error('The server returned malformed planned-enrollment data.')
      }
      if (sequence === requestSequence.current) {
        setState({ status: 'loaded', enrollments: response })
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
    const refresh = () => { void load() }
    window.addEventListener('sua:plan-changed', refresh)
    return () => {
      requestSequence.current += 1
      window.removeEventListener('sua:plan-changed', refresh)
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

  async function remove(enrollmentId: number) {
    setRemovingId(enrollmentId)
    setRemovalError('')
    try {
      await removePlannedEnrollment(enrollmentId)
      await load()
    } catch (error) {
      setRemovalError(
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
          Your planned enrollments by semester. These totals include planned
          credits only; backend credit-limit checks include planned and active courses.
        </p>
      </header>
      {state.status === 'loading' && <LoadingState label="Loading planned enrollments..." />}
      {state.status === 'error' && <ErrorMessage message={state.message} onRetry={() => void load()} />}
      {removalError && <ErrorMessage message={removalError} />}
      {state.status === 'loaded' && state.enrollments.length === 0 && (
        <EmptyState message="You have no planned enrollments." />
      )}
      {groups.map(([semester, enrollments]) => (
        <section className="panel" key={semester}>
          <h2>{semester}</h2>
          <p>{enrollments.reduce((total, item) => total + item.credits, 0)} planned credits</p>
          {enrollments.map((item) => (
            <article className="course-card" key={item.id}>
              <div className="course-card-body">
                <p className="course-card-title">{item.course_code} · {item.course_name}</p>
                <p className="course-card-meta">{item.credits} credits · Planned</p>
              </div>
              <button
                type="button"
                className="secondary-button"
                onClick={() => void remove(item.id)}
                disabled={removingId !== null}
              >
                {removingId === item.id ? 'Removing...' : 'Remove'}
              </button>
            </article>
          ))}
        </section>
      ))}
    </div>
  )
}
