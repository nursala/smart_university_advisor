import { Outlet } from 'react-router-dom'
import { useLocation } from 'react-router-dom'
import Navbar from './Navbar'

export default function AppLayout() {
  const location = useLocation()
  const isChat = location.pathname === '/chat'
  return (
    <div className={`app-layout ${isChat ? 'app-layout-chat' : ''}`}>
      <Navbar />
      <main className={`app-shell ${isChat ? 'app-shell-chat' : ''}`}>
        <Outlet />
      </main>
    </div>
  )
}
