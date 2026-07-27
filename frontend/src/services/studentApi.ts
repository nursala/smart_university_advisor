import { apiRequest } from './api'
import type {
  AcademicSummary,
  AvailableCourseListItem,
  CourseRecommendationsResponse,
  RiskAnalysisResponse,
  SemesterPlanResponse,
  StudentProfile,
} from '../types/student'
import type { DifficultyLevel } from '../types/course'

export function getProfile(studentId: number) {
  return apiRequest<StudentProfile>(`/students/${studentId}/profile`)
}

export function getAcademicSummary(studentId: number) {
  return apiRequest<AcademicSummary>(`/students/${studentId}/academic-summary`)
}

export function getAvailableCourses(studentId: number) {
  return apiRequest<AvailableCourseListItem[]>(
    `/students/${studentId}/available-courses`,
  )
}

export function getCourseRecommendations(
  studentId: number,
  options: { preferredDifficulty?: DifficultyLevel; maxRecommendations?: number } = {},
) {
  return apiRequest<CourseRecommendationsResponse>(
    `/students/${studentId}/course-recommendations`,
    {
      method: 'POST',
      body: {
        preferred_difficulty: options.preferredDifficulty,
        max_recommendations: options.maxRecommendations,
      },
    },
  )
}

export function buildSemesterPlan(
  studentId: number,
  options: { maxCredits?: number; preferredDifficulty?: DifficultyLevel } = {},
) {
  return apiRequest<SemesterPlanResponse>(
    `/students/${studentId}/semester-plan`,
    {
      method: 'POST',
      body: {
        max_credits: options.maxCredits,
        preferred_difficulty: options.preferredDifficulty,
      },
    },
  )
}

export function analyzeRisk(studentId: number, courseIds: number[]) {
  return apiRequest<RiskAnalysisResponse>(
    `/students/${studentId}/risk-analysis`,
    {
      method: 'POST',
      body: { course_ids: courseIds },
    },
  )
}
