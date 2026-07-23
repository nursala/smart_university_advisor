import { apiRequest } from './api'
import type { AgentQueryResponse } from '../types/agent'

export function queryAgent(studentId: number, message: string, token: string) {
  return apiRequest<AgentQueryResponse>('/agent/query', {
    method: 'POST',
    token,
    body: { student_id: studentId, message },
  })
}
