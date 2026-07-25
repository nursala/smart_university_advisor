import { useState } from 'react'
import type { FormEvent } from 'react'
import { Link, useNavigate } from 'react-router-dom'
import { useAuth } from '../auth/AuthContext'
import { login as loginRequest } from '../services/authApi'
import { ApiError } from '../services/api'

export default function LoginPage() {
  const { login } = useAuth()
  const navigate = useNavigate()

  const [email, setEmail] = useState('')
  const [password, setPassword] = useState('')
  const [showPassword, setShowPassword] = useState(false)
  const [error, setError] = useState('')
  const [loading, setLoading] = useState(false)

  async function handleSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault()

    if (!/^\S+@\S+\.\S+$/.test(email) || !password) {
      setError('Enter a valid email and password.')
      return
    }

    setLoading(true)
    setError('')

    try {
      const { token, user } = await loginRequest(email, password)
      login(token, user)
      navigate(user.role === 'student' ? '/dashboard' : '/courses', { replace: true })
    } catch (err) {
      setError(
        err instanceof ApiError ? err.message : 'Unable to sign in. Please try again.',
      )
    } finally {
      setLoading(false)
    }
  }

  return (
    <main className="auth-shell">
      <form className="auth-card" onSubmit={handleSubmit} noValidate>
        <h1>Smart University Advisor</h1>
        <p>Sign in to your account.</p>

        <label>
          Email
          <input
            type="email"
            autoComplete="email"
            value={email}
            onChange={(event) => setEmail(event.target.value)}
          />
        </label>

        <label>
          Password
          <input
            type={showPassword ? 'text' : 'password'}
            autoComplete="current-password"
            value={password}
            onChange={(event) => setPassword(event.target.value)}
          />
        </label>

        <button
          type="button"
          className="link-button"
          onClick={() => setShowPassword((value) => !value)}
        >
          {showPassword ? 'Hide password' : 'Show password'}
        </button>

        {error && (
          <p className="form-error" role="alert">
            {error}
          </p>
        )}

        <button type="submit" disabled={loading}>
          {loading ? 'Signing in...' : 'Login'}
        </button>

        <Link to="/register">Don't have an account? Register</Link>
      </form>
    </main>
  )
}
