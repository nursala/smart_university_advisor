import { apiRequest } from './api'
import type {
  AcademicSummary,
  AvailableCourse,
  CourseRecommendationsResponse,
  RiskAnalysisResponse,
  SemesterPlanResponse,
  StudentProfile,
} from '../types/student'
import type { DifficultyLevel } from '../types/course'

export function getProfile(studentId: number, token: string) {
  return apiRequest<StudentProfile>(`/students/${studentId}/profile`, { token })
}

export function getAcademicSummary(studentId: number, token: string) {
  return apiRequest<AcademicSummary>(`/students/${studentId}/academic-summary`, {
    token,
  })
}

export function getAvailableCourses(studentId: number, token: string) {
  return apiRequest<AvailableCourse[]>(
    `/students/${studentId}/available-courses`,
    { token },
  )
}

export function getCourseRecommendations(
  studentId: number,
  token: string,
  options: { preferredDifficulty?: DifficultyLevel; maxRecommendations?: number } = {},
) {
  return apiRequest<CourseRecommendationsResponse>(
    `/students/${studentId}/course-recommendations`,
    {
      method: 'POST',
      token,
      body: {
        preferred_difficulty: options.preferredDifficulty,
        max_recommendations: options.maxRecommendations,
      },
    },
  )
}

export function buildSemesterPlan(
  studentId: number,
  token: string,
  options: { maxCredits?: number; preferredDifficulty?: DifficultyLevel } = {},
) {
  return apiRequest<SemesterPlanResponse>(
    `/students/${studentId}/semester-plan`,
    {
      method: 'POST',
      token,
      body: {
        max_credits: options.maxCredits,
        preferred_difficulty: options.preferredDifficulty,
      },
    },
  )
}

export function analyzeRisk(studentId: number, token: string, courseIds: number[]) {
  return apiRequest<RiskAnalysisResponse>(
    `/students/${studentId}/risk-analysis`,
    {
      method: 'POST',
      token,
      body: { course_ids: courseIds },
    },
  )
}
