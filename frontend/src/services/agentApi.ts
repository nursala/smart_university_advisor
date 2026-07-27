import { apiRequest } from './api'
import type { AgentQueryResponse } from '../types/agent'

export function queryAgent(message: string) {
  return apiRequest<AgentQueryResponse>('/agent/query', {
    method: 'POST',
    body: { message },
  })
}
