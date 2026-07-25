import { BrowserRouter, Navigate, Route, Routes } from 'react-router-dom'
import { useAuth } from './auth/AuthContext'
import ProtectedRoute from './auth/ProtectedRoute'
import GuestRoute from './auth/GuestRoute'
import AppLayout from './components/AppLayout'
import LoginPage from './pages/LoginPage'
import RegisterPage from './pages/RegisterPage'
import ChatPage from './pages/ChatPage'
import CoursesPage from './pages/CoursesPage'
import ProfilePage from './pages/ProfilePage'
import MyPlanPage from './pages/MyPlanPage'
import './App.css'

function RoleHome() {
  const { isAuthenticated, isRestoring, restorationError, user } = useAuth()
  if (isRestoring || restorationError) return <Navigate to="/profile" replace />
  if (!isAuthenticated) return <Navigate to="/login" replace />
  return <Navigate to={user?.role === 'student' ? '/profile' : '/courses'} replace />
}

export default function App() {
  return (
    <BrowserRouter>
      <Routes>
        <Route element={<GuestRoute />}>
          <Route path="/login" element={<LoginPage />} />
          <Route path="/register" element={<RegisterPage />} />
        </Route>
        <Route element={<ProtectedRoute />}>
          <Route element={<AppLayout />}>
            <Route path="/dashboard" element={<RoleHome />} />
            <Route path="/chat" element={<ChatPage />} />
            <Route path="/courses" element={<CoursesPage />} />
            <Route path="/profile" element={<ProfilePage />} />
            <Route path="/my-plan" element={<MyPlanPage />} />
          </Route>
        </Route>
        <Route path="/" element={<RoleHome />} />
        <Route path="*" element={<RoleHome />} />
      </Routes>
    </BrowserRouter>
  )
}
