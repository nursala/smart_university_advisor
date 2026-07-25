import { Navigate, Outlet } from 'react-router-dom'
import { useAuth } from './AuthContext'

export default function GuestRoute() {
  const {
    isAuthenticated,
    isRestoring,
    restorationError,
    retryRestoration,
    logout,
    user,
  } = useAuth()

  if (isRestoring) return <main className="auth-state">Validating your session...</main>
  if (restorationError) {
    return (
      <main className="auth-state">
        <h1>Unable to validate your session</h1>
        <p>{restorationError}</p>
        <div className="actions">
          <button type="button" onClick={retryRestoration}>Retry</button>
          <button type="button" className="secondary-button" onClick={logout}>
            Return to Login
          </button>
        </div>
      </main>
    )
  }

  return isAuthenticated
    ? <Navigate to={user?.role === 'student' ? '/profile' : '/courses'} replace />
    : <Outlet />
}
