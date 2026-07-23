import { Navigate, Outlet } from 'react-router-dom'
import { useAuth } from './AuthContext'

export default function GuestRoute() {
  const { isAuthenticated } = useAuth()

  return isAuthenticated ? <Navigate to="/dashboard" replace /> : <Outlet />
}
