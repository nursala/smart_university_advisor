import { useState } from 'react'
import type { FormEvent } from 'react'
import { Link, useNavigate } from 'react-router-dom'
import { register as registerRequest } from '../services/authApi'
import { ApiError } from '../services/api'

type FieldErrors = Partial<
  Record<'name' | 'email' | 'password' | 'confirmPassword', string>
>

export default function RegisterPage() {
  const navigate = useNavigate()

  const [name, setName] = useState('')
  const [email, setEmail] = useState('')
  const [password, setPassword] = useState('')
  const [confirmPassword, setConfirmPassword] = useState('')
  const [showPassword, setShowPassword] = useState(false)
  const [errors, setErrors] = useState<FieldErrors>({})
  const [message, setMessage] = useState('')
  const [loading, setLoading] = useState(false)

  async function handleSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault()

    const nextErrors: FieldErrors = {}
    if (!name.trim()) nextErrors.name = 'Full name is required.'
    if (!email.trim()) nextErrors.email = 'Email is required.'
    else if (!/^\S+@\S+\.\S+$/.test(email)) {
      nextErrors.email = 'Enter a valid email address.'
    }
    if (!password) nextErrors.password = 'Password is required.'
    else if (password.length < 8) {
      nextErrors.password = 'Password must be at least 8 characters.'
    }
    if (!confirmPassword) nextErrors.confirmPassword = 'Confirm your password.'
    else if (confirmPassword !== password) {
      nextErrors.confirmPassword = 'Passwords do not match.'
    }

    setErrors(nextErrors)
    setMessage('')
    if (Object.keys(nextErrors).length > 0) return

    setLoading(true)
    try {
      await registerRequest(name.trim(), email.trim(), password)
      setMessage('Account created successfully. Please sign in.')
      setTimeout(() => navigate('/login'), 900)
    } catch (err) {
      setMessage(
        err instanceof ApiError
          ? err.message
          : 'Unable to create account. Please try again.',
      )
    } finally {
      setLoading(false)
    }
  }

  return (
    <main className="auth-shell">
      <form className="auth-card" onSubmit={handleSubmit} noValidate>
        <h1>Smart University Advisor</h1>
        <p>Create your student account to begin planning.</p>

        <label>
          Full Name
          <input value={name} onChange={(event) => setName(event.target.value)} />
          {errors.name && <span>{errors.name}</span>}
        </label>

        <label>
          Email
          <input
            type="email"
            value={email}
            onChange={(event) => setEmail(event.target.value)}
          />
          {errors.email && <span>{errors.email}</span>}
        </label>

        <label>
          Password
          <input
            type={showPassword ? 'text' : 'password'}
            value={password}
            onChange={(event) => setPassword(event.target.value)}
          />
          {errors.password && <span>{errors.password}</span>}
        </label>

        <label>
          Confirm Password
          <input
            type={showPassword ? 'text' : 'password'}
            value={confirmPassword}
            onChange={(event) => setConfirmPassword(event.target.value)}
          />
          {errors.confirmPassword && <span>{errors.confirmPassword}</span>}
        </label>

        <button
          type="button"
          className="link-button"
          onClick={() => setShowPassword((value) => !value)}
        >
          {showPassword ? 'Hide passwords' : 'Show passwords'}
        </button>

        {message && (
          <p role="alert" className="form-error">
            {message}
          </p>
        )}

        <button disabled={loading}>
          {loading ? 'Creating account...' : 'Register'}
        </button>

        <Link to="/login">Already have an account? Log in</Link>
      </form>
    </main>
  )
}
