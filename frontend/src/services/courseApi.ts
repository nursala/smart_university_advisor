import { apiRequest } from './api'
import type { CourseDetails, CourseFilters, CourseSummary } from '../types/course'

export function getCourses(filters: CourseFilters = {}) {
  const params = new URLSearchParams()
  if (filters.department) params.set('department', filters.department)
  if (filters.difficulty) params.set('difficulty', filters.difficulty)
  if (filters.credits) params.set('credits', filters.credits)
  if (filters.instructor) params.set('instructor', filters.instructor)

  const query = params.toString()
  return apiRequest<CourseSummary[]>(`/courses${query ? `?${query}` : ''}`)
}

export function getCourseDetails(courseId: number) {
  return apiRequest<CourseDetails>(`/courses/${courseId}/details`)
}
