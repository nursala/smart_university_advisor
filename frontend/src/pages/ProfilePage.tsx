import { useEffect, useState } from 'react'
import { useNavigate } from 'react-router-dom'
import { useAuth } from '../auth/AuthContext'
import { getAcademicSummary, getProfile } from '../services/studentApi'
import { isAuthError } from '../services/api'
import LoadingState from '../components/LoadingState'
import ErrorMessage from '../components/ErrorMessage'
import EmptyState from '../components/EmptyState'
import type { AcademicSummary, StudentProfile } from '../types/student'

type ProfileState =
  | { status: 'loading' }
  | { status: 'error'; message: string }
  | { status: 'loaded'; profile: StudentProfile; summary: AcademicSummary }

export default function ProfilePage() {
  const { user, token, logout } = useAuth()
  const navigate = useNavigate()
  const studentId = user?.student_id ?? null

  const [state, setState] = useState<ProfileState>({ status: 'loading' })

  useEffect(() => {
    if (studentId === null || !token) return
    let ignore = false

    setState({ status: 'loading' })

    Promise.all([getProfile(studentId, token), getAcademicSummary(studentId, token)])
      .then(([profile, summary]) => {
        if (ignore) return
        setState({ status: 'loaded', profile, summary })
      })
      .catch((error: unknown) => {
        if (ignore) return
        if (isAuthError(error)) {
          logout()
          navigate('/login', { replace: true })
          return
        }
        setState({
          status: 'error',
          message: error instanceof Error ? error.message : 'Unable to load your profile.',
        })
      })

    return () => {
      ignore = true
    }
  }, [studentId, token, logout, navigate])

  if (studentId === null) {
    return (
      <EmptyState message="Your account isn't linked to a student profile. Contact your advisor if you believe this is a mistake." />
    )
  }

  return (
    <div className="page">
      <header className="page-header">
        <h1>Profile</h1>
        <p>Your academic identity and progress at a glance.</p>
      </header>

      {state.status === 'loading' && <LoadingState label="Loading your profile..." />}

      {state.status === 'error' && (
        <ErrorMessage message={state.message} onRetry={() => setState({ status: 'loading' })} />
      )}

      {state.status === 'loaded' && (
        <>
          <section className="panel">
            <h2>Identity</h2>
            <dl className="detail-list">
              <div>
                <dt>Name</dt>
                <dd>{state.profile.name}</dd>
              </div>
              <div>
                <dt>Email</dt>
                <dd>{state.profile.email}</dd>
              </div>
              <div>
                <dt>Student number</dt>
                <dd>{state.profile.student_number}</dd>
              </div>
              <div>
                <dt>Department</dt>
                <dd>{state.profile.department}</dd>
              </div>
              <div>
                <dt>Year level</dt>
                <dd>{state.profile.year_level}</dd>
              </div>
              <div>
                <dt>Weekly credit limit</dt>
                <dd>{state.profile.max_weekly_credits}</dd>
              </div>
            </dl>
          </section>

          <section className="stat-grid" aria-label="Academic summary">
            <StatCard
              label="Current GPA"
              value={
                state.summary.current_gpa === null
                  ? 'No grades yet'
                  : state.summary.current_gpa.toFixed(2)
              }
            />
            <StatCard label="Completed credits" value={state.summary.completed_credits} />
            <StatCard label="Completed courses" value={state.summary.completed_courses_count} />
            <StatCard label="Active courses" value={state.summary.active_courses_count} />
            <StatCard label="Failed courses" value={state.summary.failed_courses_count} />
          </section>
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
