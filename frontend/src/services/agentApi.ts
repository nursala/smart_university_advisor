import { apiRequest } from './api'
import type { AgentQueryResponse, EnrollmentConfirmationResponse } from '../types/agent'

export function queryAgent(message: string) {
  return apiRequest<AgentQueryResponse>('/agent/query', {
    method: 'POST',
    body: { message },
  })
}

export function confirmEnrollment(confirmationId: string) {
  return apiRequest<EnrollmentConfirmationResponse>('/agent/query', {
    method: 'POST',
    body: {
      message: 'Confirm the pending enrollment',
      confirmation_id: confirmationId,
    },
  })
}
