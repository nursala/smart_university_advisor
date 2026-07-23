export type AgentQueryResponse = {
  student_id?: number
  message?: string
  answer?: string
  tools_used?: string[]
  status?: string
  error?: string
}
