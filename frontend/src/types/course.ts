export type DifficultyLevel = 'easy' | 'medium' | 'hard'

export type CourseSummary = {
  id: number
  code: string
  name: string
  department: string
  credits: number
  difficulty_level: DifficultyLevel
}

export type CourseFilters = {
  department?: string
  difficulty?: DifficultyLevel | ''
  credits?: string
  instructor?: string
}

export type CoursePrerequisite = {
  code: string
  name: string
  minimum_grade: number
}

export type CourseDetails = {
  id: number
  code: string
  name: string
  department: string
  credits: number
  difficulty_level: DifficultyLevel
  estimated_weekly_hours: number
  description: string | null
  instructor_id: number | null
  instructor_name: string | null
  prerequisites: CoursePrerequisite[]
}
