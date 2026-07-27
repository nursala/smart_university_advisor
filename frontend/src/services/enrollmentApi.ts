import { apiRequest } from './api'
import type { PlannedEnrollment } from '../types/student'

export function getPlannedEnrollments() {
  return apiRequest<PlannedEnrollment[]>('/enrollments/planned')
}

export function createPlannedEnrollment(courseId: number, semester: string) {
  return apiRequest<PlannedEnrollment>('/enrollments', {
    method: 'POST',
    body: { course_id: courseId, semester },
  })
}

export function removePlannedEnrollment(enrollmentId: number) {
  return apiRequest<{ id: number; deleted: boolean; current_gpa: number | null }>(
    `/enrollments/${enrollmentId}`,
    { method: 'DELETE' },
  )
}
