import { useEffect, useState } from 'react'
import { useAuth } from '../auth/AuthContext'
import { getAcademicSummary, getProfile } from '../services/studentApi'
import { updateCurrentUser } from '../services/userApi'
import LoadingState from '../components/LoadingState'
import ErrorMessage from '../components/ErrorMessage'
import EmptyState from '../components/EmptyState'
import type { AcademicSummary, StudentProfile } from '../types/student'

type ProfileState =
  | { status: 'loading' }
  | { status: 'error'; message: string }
  | { status: 'loaded'; profile: StudentProfile; summary: AcademicSummary }

export default function ProfilePage() {
  const { user, updateUser } = useAuth()
  const studentId = user?.student_id ?? null

  const [state, setState] = useState<ProfileState>({ status: 'loading' })
  const [reloadKey, setReloadKey] = useState(0)
  const [isEditing, setIsEditing] = useState(false)
  const [name, setName] = useState(user?.name ?? '')
  const [email, setEmail] = useState(user?.email ?? '')
  const [saveError, setSaveError] = useState('')
  const [isSaving, setIsSaving] = useState(false)

  useEffect(() => {
    if (studentId === null) return
    let ignore = false

    setState({ status: 'loading' })

    Promise.all([getProfile(studentId), getAcademicSummary(studentId)])
      .then(([profile, summary]) => {
        if (ignore) return
        setState({ status: 'loaded', profile, summary })
      })
      .catch((error: unknown) => {
        if (ignore) return
        setState({
          status: 'error',
          message: error instanceof Error ? error.message : 'Unable to load your profile.',
        })
      })

    return () => {
      ignore = true
    }
  }, [studentId, reloadKey])

  async function saveIdentity() {
    if (!name.trim() || !email.trim()) {
      setSaveError('Name and email are required.')
      return
    }
    setIsSaving(true)
    setSaveError('')
    try {
      const updated = await updateCurrentUser({ name: name.trim(), email: email.trim() })
      updateUser(updated)
      setIsEditing(false)
      setReloadKey((value) => value + 1)
    } catch (error) {
      setSaveError(error instanceof Error ? error.message : 'Unable to update your profile.')
    } finally {
      setIsSaving(false)
    }
  }

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
        <ErrorMessage message={state.message} onRetry={() => setReloadKey((value) => value + 1)} />
      )}

      {state.status === 'loaded' && (
        <>
          <section className="panel">
            <h2>Identity</h2>
            {isEditing ? (
              <div className="profile-edit">
                <label>
                  Name
                  <input value={name} onChange={(event) => setName(event.target.value)} />
                </label>
                <label>
                  Email
                  <input type="email" value={email} onChange={(event) => setEmail(event.target.value)} />
                </label>
                {saveError && <p className="form-error" role="alert">{saveError}</p>}
                <div className="actions">
                  <button type="button" onClick={saveIdentity} disabled={isSaving}>
                    {isSaving ? 'Saving...' : 'Save'}
                  </button>
                  <button type="button" className="secondary-button" onClick={() => setIsEditing(false)}>
                    Cancel
                  </button>
                </div>
              </div>
            ) : (
              <button type="button" className="secondary-button" onClick={() => setIsEditing(true)}>
                Edit name and email
              </button>
            )}
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
                  ? 'Not available'
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
