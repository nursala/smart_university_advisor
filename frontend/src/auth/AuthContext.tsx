import { createContext, useContext, useMemo, useState } from 'react'
import type { ReactNode } from 'react'
import type { AuthUser } from '../types/auth'

type StoredAuth = {
  token: string
  user: AuthUser
}

type Auth = {
  user: AuthUser | null
  token: string | null
  isAuthenticated: boolean
  login: (token: string, user: AuthUser) => void
  logout: () => void
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

  const value = useMemo<Auth>(
    () => ({
      user: state?.user ?? null,
      token: state?.token ?? null,
      isAuthenticated: Boolean(state),
      login: (token, user) => {
        const next = { token, user }
        localStorage.setItem(STORAGE_KEY, JSON.stringify(next))
        setState(next)
      },
      logout: () => {
        localStorage.removeItem(STORAGE_KEY)
        setState(null)
      },
    }),
    [state],
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
