import { createContext, useCallback, useContext, useEffect, useMemo, useState } from 'react'
import type { ReactNode } from 'react'
import type { AuthUser } from '../types/auth'
import { ApiError, configureApiAuth } from '../services/api'
import { validateCurrentUser } from '../services/userApi'
import { useChat } from '../chat/ChatContext'

type StoredAuth = { token: string; user: AuthUser }
type AuthStatus = 'restoring' | 'authenticated' | 'guest' | 'unavailable'
type AuthState = {
  status: AuthStatus
  token: string | null
  user: AuthUser | null
  message?: string
}

type Auth = {
  user: AuthUser | null
  token: string | null
  isAuthenticated: boolean
  isRestoring: boolean
  restorationError: string | null
  login: (token: string, user: AuthUser) => void
  logout: () => void
  retryRestoration: () => void
  updateUser: (user: AuthUser) => void
}

const AuthContextInstance = createContext<Auth | null>(null)
const STORAGE_KEY = 'sua_auth'

function readCandidate(): StoredAuth | null {
  localStorage.removeItem(STORAGE_KEY)
  try {
    const parsed = JSON.parse(sessionStorage.getItem(STORAGE_KEY) ?? 'null')
    return parsed?.token && parsed?.user ? parsed : null
  } catch {
    sessionStorage.removeItem(STORAGE_KEY)
    return null
  }
}

export function AuthProvider({ children }: { children: ReactNode }) {
  const { resetChat } = useChat()
  const [state, setState] = useState<AuthState>(() => {
    const candidate = readCandidate()
    return candidate
      ? { status: 'restoring', token: candidate.token, user: null }
      : { status: 'guest', token: null, user: null }
  })
  const [retryKey, setRetryKey] = useState(0)

  const clearAuthentication = useCallback(() => {
    sessionStorage.removeItem(STORAGE_KEY)
    localStorage.removeItem(STORAGE_KEY)
    configureApiAuth(null, null)
    resetChat()
    setState({ status: 'guest', token: null, user: null })
  }, [resetChat])

  useEffect(() => {
    configureApiAuth(state.token, clearAuthentication)
    return () => configureApiAuth(null, null)
  }, [state.token, clearAuthentication])

  useEffect(() => {
    if (state.status !== 'restoring' || !state.token) return
    let ignore = false
    validateCurrentUser(state.token)
      .then((user) => {
        if (ignore) return
        const restored = { token: state.token!, user }
        sessionStorage.setItem(STORAGE_KEY, JSON.stringify(restored))
        setState({ status: 'authenticated', ...restored })
      })
      .catch((error: unknown) => {
        if (ignore) return
        if (error instanceof ApiError && (error.status === 401 || error.status === 403)) {
          clearAuthentication()
          return
        }
        setState({
          status: 'unavailable',
          token: state.token,
          user: null,
          message: error instanceof Error
            ? error.message
            : 'Authentication could not be validated.',
        })
      })
    return () => { ignore = true }
  }, [state.status, state.token, retryKey, clearAuthentication])

  const value = useMemo<Auth>(() => ({
    user: state.user,
    token: state.token,
    isAuthenticated: state.status === 'authenticated',
    isRestoring: state.status === 'restoring',
    restorationError: state.status === 'unavailable'
      ? state.message ?? 'Authentication could not be validated.'
      : null,
    login: (token, user) => {
      const authenticated = { token, user }
      localStorage.removeItem(STORAGE_KEY)
      sessionStorage.setItem(STORAGE_KEY, JSON.stringify(authenticated))
      configureApiAuth(token, clearAuthentication)
      resetChat()
      setState({ status: 'authenticated', ...authenticated })
    },
    logout: clearAuthentication,
    retryRestoration: () => {
      setState((current) => current.token
        ? { status: 'restoring', token: current.token, user: null }
        : { status: 'guest', token: null, user: null })
      setRetryKey((value) => value + 1)
    },
    updateUser: (user) => {
      setState((current) => {
        if (current.status !== 'authenticated' || !current.token) return current
        sessionStorage.setItem(STORAGE_KEY, JSON.stringify({ token: current.token, user }))
        return { ...current, user }
      })
    },
  }), [state, clearAuthentication, resetChat])

  return <AuthContextInstance.Provider value={value}>{children}</AuthContextInstance.Provider>
}

export function useAuth(): Auth {
  const context = useContext(AuthContextInstance)
  if (!context) throw new Error('useAuth must be used within an AuthProvider')
  return context
}
