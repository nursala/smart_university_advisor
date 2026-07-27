import type { DifficultyLevel } from './course'

export type PlannedEnrollmentListItem = {
  id: number
  student_id: number
  course_id: number
  course_code: string
  course_name: string
  credits: number
  difficulty_level: DifficultyLevel
  semester: string
  status: 'planned'
  enrolled_at: string
}

export type EnrollmentCreationResponse = {
  id: number
  student_id: number
  course_id: number
  semester: string
  status: 'planned'
  enrolled_at: string
}
