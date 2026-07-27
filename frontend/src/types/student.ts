import type { DifficultyLevel } from './course'

export type StudentProfile = {
  id: number
  name: string
  email: string
  student_number: string
  department: string
  year_level: number
  current_gpa: number | null
  max_weekly_credits: number
}

export type AcademicSummary = {
  completed_courses_count: number
  active_courses_count: number
  failed_courses_count: number
  completed_credits: number
  current_gpa: number | null
}

export type AvailableCourseListItem = {
  id: number
  code: string
  name: string
  department: string
  credits: number
  difficulty_level: DifficultyLevel
}

export type CourseRecommendation = {
  id: number
  code: string
  name: string
  credits: number
  difficulty_level: DifficultyLevel
  reason: string
}

export type CourseRecommendationsResponse = {
  student_id: number
  recommendations: CourseRecommendation[]
}

export type PlannedCourse = {
  id: number
  code: string
  name: string
  credits: number
  difficulty_level: DifficultyLevel
  estimated_weekly_hours: number
  reason: string
}

export type SemesterPlanResponse = {
  student_id: number
  max_credits: number
  total_credits: number
  estimated_weekly_hours: number
  courses: PlannedCourse[]
}

export type RiskLevel = 'Low' | 'Medium' | 'High'

export type RiskAnalysisResponse = {
  student_id: number
  risk_level: RiskLevel
  total_credits: number
  estimated_weekly_hours: number
  hard_courses_count: number
  reasons: string[]
  recommendations: string[]
}
