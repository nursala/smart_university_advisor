export type Role = 'student' | 'advisor' | 'admin'

export type AuthUser = {
  id: number
  name: string
  email: string
  role: Role
  student_id: number | null
}
