import { apiRequest } from './api'
import type {
  EnrollmentCreationResponse,
  PlannedEnrollmentListItem,
} from '../types/enrollment'

export function getPlannedEnrollments() {
  return apiRequest<PlannedEnrollmentListItem[]>('/enrollments/planned')
}

export function createPlannedEnrollment(courseId: number, semester: string) {
  return apiRequest<EnrollmentCreationResponse>('/enrollments', {
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
