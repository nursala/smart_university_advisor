import { Navigate, Outlet } from 'react-router-dom'
import { useAuth } from './AuthContext'

export default function ProtectedRoute() {
  const {
    isAuthenticated,
    isRestoring,
    restorationError,
    retryRestoration,
    logout,
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
  return isAuthenticated ? <Outlet /> : <Navigate to="/login" replace />
}
