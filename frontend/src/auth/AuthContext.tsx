import { createContext, useCallback, useContext, useEffect, useMemo, useState } from 'react'
import type { ReactNode } from 'react'
import type { AuthUser } from '../types/auth'
import { configureApiAuth } from '../services/api'

type StoredAuth = {
  token: string
  user: AuthUser
}

type Auth = {
  user: AuthUser | null
  token: string | null
  isAuthenticated: boolean
  isRestoring: boolean
  login: (token: string, user: AuthUser) => void
  logout: () => void
  updateUser: (user: AuthUser) => void
}

const AuthContextInstance = createContext<Auth | null>(null)
const STORAGE_KEY = 'sua_auth'

function readStoredAuth(): StoredAuth | null {
  try {
    return JSON.parse(localStorage.getItem(STORAGE_KEY) ?? 'null')
  } catch {
    return null
  }
}

export function AuthProvider({ children }: { children: ReactNode }) {
  const [state, setState] = useState<StoredAuth | null>(readStoredAuth)
  const logout = useCallback(() => {
    localStorage.removeItem(STORAGE_KEY)
    setState(null)
  }, [])

  useEffect(() => {
    configureApiAuth(state?.token ?? null, logout)
    return () => configureApiAuth(null, null)
  }, [state?.token, logout])

  const value = useMemo<Auth>(
    () => ({
      user: state?.user ?? null,
      token: state?.token ?? null,
      isAuthenticated: Boolean(state),
      isRestoring: false,
      login: (token, user) => {
        const next = { token, user }
        localStorage.setItem(STORAGE_KEY, JSON.stringify(next))
        setState(next)
      },
      logout,
      updateUser: (user) => {
        setState((current) => {
          if (!current) return current
          const next = { ...current, user }
          localStorage.setItem(STORAGE_KEY, JSON.stringify(next))
          return next
        })
      },
    }),
    [state, logout],
  )

  return (
    <AuthContextInstance.Provider value={value}>
      {children}
    </AuthContextInstance.Provider>
  )
}

export function useAuth(): Auth {
  const context = useContext(AuthContextInstance)
  if (!context) {
    throw new Error('useAuth must be used within an AuthProvider')
  }
  return context
}
