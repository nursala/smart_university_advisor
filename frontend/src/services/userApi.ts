import { apiRequest } from './api'
import type { AuthUser } from '../types/auth'

export function updateCurrentUser(input: { name: string; email: string }) {
  return apiRequest<AuthUser>('/users/me', { method: 'PATCH', body: input })
}

export function validateCurrentUser(token: string) {
  return apiRequest<AuthUser>('/users/me', {
    token,
    suppressUnauthorizedHandler: true,
  })
}
