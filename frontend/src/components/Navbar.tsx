import { NavLink, useNavigate } from 'react-router-dom'
import { useAuth } from '../auth/AuthContext'

const navItems = [
  { to: '/dashboard', label: 'Dashboard' },
  { to: '/chat', label: 'AI Advisor' },
  { to: '/courses', label: 'Courses' },
  { to: '/profile', label: 'Profile' },
]

export default function Navbar() {
  const { user, logout } = useAuth()
  const navigate = useNavigate()

  function handleLogout() {
    logout()
    navigate('/login', { replace: true })
  }

  return (
    <header className="app-header">
      <div className="app-header-brand">Smart University Advisor</div>

      <nav className="app-nav" aria-label="Main navigation">
        {navItems.map((item) => (
          <NavLink
            key={item.to}
            to={item.to}
            className={({ isActive }) => (isActive ? 'active' : undefined)}
          >
            {item.label}
          </NavLink>
        ))}
      </nav>

      <div className="app-header-user">
        <span className="app-header-name">{user?.name}</span>
        {user?.role && <span className="app-header-role">{user.role}</span>}
        <button type="button" className="app-header-logout" onClick={handleLogout}>
          Logout
        </button>
      </div>
    </header>
  )
}
