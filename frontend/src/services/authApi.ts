import { apiRequest } from './api'
import type { AuthUser } from '../types/auth'

export type AuthResponse = { token: string; user: AuthUser }

export function login(email: string, password: string) {
  return apiRequest<AuthResponse>('/auth/login', {
    method: 'POST',
    body: { email, password },
  })
}

export function register(name: string, email: string, password: string) {
  return apiRequest<AuthResponse>('/auth/register', {
    method: 'POST',
    body: { name, email, password },
  })
}
