import { useCallback, useEffect, useMemo, useState } from 'react'
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

export default function MyPlanPage() {
  const { user } = useAuth()
  const [state, setState] = useState<State>({ status: 'loading' })
  const [removingId, setRemovingId] = useState<number | null>(null)

  const load = useCallback(() => {
    if (user?.student_id == null) return
    setState({ status: 'loading' })
    getPlannedEnrollments()
      .then((enrollments) => setState({ status: 'loaded', enrollments }))
      .catch((error: unknown) => setState({
        status: 'error',
        message: error instanceof Error ? error.message : 'Unable to load My Plan.',
      }))
  }, [user?.student_id])

  useEffect(() => {
    load()
    window.addEventListener('sua:plan-changed', load)
    return () => window.removeEventListener('sua:plan-changed', load)
  }, [load])

  const groups = useMemo(() => {
    if (state.status !== 'loaded') return []
    const grouped = new Map<string, PlannedEnrollment[]>()
    state.enrollments.forEach((item) =>
      grouped.set(item.semester, [...(grouped.get(item.semester) ?? []), item]),
    )
    return Array.from(grouped)
  }, [state])

  async function remove(enrollmentId: number) {
    setRemovingId(enrollmentId)
    try {
      await removePlannedEnrollment(enrollmentId)
      load()
    } catch (error) {
      setState({
        status: 'error',
        message: error instanceof Error ? error.message : 'Unable to remove enrollment.',
      })
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
        <p>Your confirmed planned enrollments. Official grades and statuses are advisor-managed.</p>
      </header>
      {state.status === 'loading' && <LoadingState label="Loading planned enrollments..." />}
      {state.status === 'error' && <ErrorMessage message={state.message} onRetry={load} />}
      {state.status === 'loaded' && state.enrollments.length === 0 && (
        <EmptyState message="You have no planned enrollments." />
      )}
      {groups.map(([semester, enrollments]) => (
        <section className="panel" key={semester}>
          <h2>{semester}</h2>
          <p>{enrollments.reduce((total, item) => total + item.credits, 0)} total credits</p>
          {enrollments.map((item) => (
            <article className="course-card" key={item.id}>
              <div className="course-card-body">
                <p className="course-card-title">{item.course_code} · {item.course_name}</p>
                <p className="course-card-meta">{item.credits} credits · Planned</p>
              </div>
              <button
                type="button"
                className="secondary-button"
                onClick={() => remove(item.id)}
                disabled={removingId === item.id}
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
