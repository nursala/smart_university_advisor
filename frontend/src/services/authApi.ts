import { apiRequest } from './api'
import type { AuthUser } from '../types/auth'

export function login(email: string, password: string) {
  return apiRequest<{ token: string; user: AuthUser }>('/auth/login', {
    method: 'POST',
    body: { email, password },
  })
}

export function register(name: string, email: string, password: string) {
  return apiRequest<{ user: AuthUser }>('/auth/register', {
    method: 'POST',
    body: { name, email, password },
  })
}
