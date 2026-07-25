import { Navigate, Outlet } from 'react-router-dom'
import { useAuth } from './AuthContext'

export default function GuestRoute() {
  const { isAuthenticated, user } = useAuth()

  return isAuthenticated
    ? <Navigate to={user?.role === 'student' ? '/dashboard' : '/courses'} replace />
    : <Outlet />
}
