export type AgentQueryResponse = {
  student_id?: number
  message?: string
  answer?: string
  tools_used?: string[]
  status?: string
  error?: string
  proposed_action?: PendingEnrollmentAction
}

export type PendingEnrollmentAction = {
  confirmation_id: string
  student_id: number
  course_id: number
  course_code: string
  course_name: string
  semester: string
  expires_in_seconds: number
  status: 'confirmation_required'
}

export type ConfirmedEnrollment = {
  id: number
  student_id: number
  course_id: number
  semester: string
  status: 'planned'
  enrolled_at: string
}

export type EnrollmentConfirmationResponse = {
  success: boolean
  data?: ConfirmedEnrollment
  error?: string
}
