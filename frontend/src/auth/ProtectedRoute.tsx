import { Navigate, Outlet } from 'react-router-dom'
import { useAuth } from './AuthContext'

export default function ProtectedRoute() {
  const { isAuthenticated, isRestoring } = useAuth()

  if (isRestoring) return null
  return isAuthenticated ? <Outlet /> : <Navigate to="/login" replace />
}
