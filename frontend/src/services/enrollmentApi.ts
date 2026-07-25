import { apiRequest } from './api'
import type { PlannedEnrollment } from '../types/student'

export function getPlannedEnrollments() {
  return apiRequest<PlannedEnrollment[]>('/enrollments/planned')
}

export function removePlannedEnrollment(enrollmentId: number) {
  return apiRequest<{ id: number; deleted: boolean; current_gpa: number | null }>(
    `/enrollments/${enrollmentId}`,
    { method: 'DELETE' },
  )
}
