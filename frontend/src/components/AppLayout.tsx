import { Outlet } from 'react-router-dom'
import Navbar from './Navbar'

export default function AppLayout() {
  return (
    <div className="app-layout">
      <Navbar />
      <main className="app-shell">
        <Outlet />
      </main>
    </div>
  )
}
