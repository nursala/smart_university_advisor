export type DifficultyLevel = 'easy' | 'medium' | 'hard'

export type CourseSummary = {
  id: number
  code: string
  name: string
  department: string
  credits: number
  difficulty_level: DifficultyLevel
  estimated_weekly_hours: number
  instructor_id: number | null
  instructor_name: string | null
}

export type CourseFilters = {
  department?: string
  difficulty?: DifficultyLevel | ''
  credits?: string
  instructor?: string
}

export type CoursePrerequisite = {
  id: number
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
  description: string
  instructor: {
    id: number | null
    name: string | null
    email: string | null
  }
  prerequisites: CoursePrerequisite[]
}
