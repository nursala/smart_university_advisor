import type { FormEvent, KeyboardEvent } from 'react'
import { useMemo, useState } from 'react'
import { useAuth } from '../auth/AuthContext'
import { confirmEnrollment, queryAgent } from '../services/agentApi'
import { useChat } from '../chat/ChatContext'
import EmptyState from '../components/EmptyState'

const fallbackError = 'Unable to contact the advisor. Please try again.'

export default function ChatPage() {
  const { user } = useAuth()
  const { messages, setMessages } = useChat()
  const [draft, setDraft] = useState('')
  const [isSending, setIsSending] = useState(false)
  const [formError, setFormError] = useState('')
  const trimmed = draft.trim()
  const nextId = useMemo(() => Math.max(...messages.map((item) => item.id)) + 1, [messages])

  async function submit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault()
    if (user?.student_id === null || isSending || !trimmed) return
    setIsSending(true)
    setFormError('')
    const sent = trimmed
    setMessages((current) => [...current, { id: nextId, role: 'user', content: sent }])
    setDraft('')
    try {
      const response = await queryAgent(sent)
      setMessages((current) => [...current, {
        id: nextId + 1,
        role: 'assistant',
        content: response.answer ?? response.message ?? fallbackError,
        status: response.status,
        toolsUsed: response.tools_used,
        pendingAction: response.proposed_action,
        actionState: response.proposed_action ? 'pending' : undefined,
      }])
    } catch (error) {
      setFormError(error instanceof Error ? error.message : fallbackError)
    } finally {
      setIsSending(false)
    }
  }

  function updateAction(messageId: number, update: object) {
    setMessages((current) => current.map((message) =>
      message.id === messageId ? { ...message, ...update } : message,
    ))
  }

  async function confirm(messageId: number, confirmationId: string) {
    updateAction(messageId, { actionState: 'confirming', actionError: undefined })
    try {
      await confirmEnrollment(confirmationId)
      updateAction(messageId, { actionState: 'confirmed' })
      window.dispatchEvent(new Event('sua:plan-changed'))
    } catch (error) {
      updateAction(messageId, {
        actionState: 'failed',
        actionError: error instanceof Error ? error.message : 'Unable to confirm enrollment.',
      })
    }
  }

  function keyDown(event: KeyboardEvent<HTMLTextAreaElement>) {
    if (event.key === 'Enter' && !event.shiftKey) {
      event.preventDefault()
      event.currentTarget.form?.requestSubmit()
    }
  }

  if (user?.student_id === null) {
    return <EmptyState message="AI Advisor is available only to accounts linked to a student profile." />
  }

  return (
    <section className="chat-panel" aria-labelledby="page-title">
      <header className="chat-header">
        <h1 id="page-title">AI Advisor</h1>
        <p>Ask about courses, semester planning, recommendations, and academic risk.</p>
      </header>
      <form className="chat-form" onSubmit={submit}>
        <section className="conversation" aria-label="Conversation with advisor">
          {messages.map((message) => (
            <article className={`message message-${message.role}`} key={message.id}>
              <p className="message-label">{message.role === 'assistant' ? 'Advisor' : 'You'}</p>
              <p className="message-content">{message.content}</p>
              {message.toolsUsed?.length ? <p className="message-status">Tools: {message.toolsUsed.join(', ')}</p> : null}
              {message.pendingAction && (
                <section className="confirmation-card" aria-label="Pending enrollment confirmation">
                  <h3>Confirm planned enrollment</h3>
                  <p>{message.pendingAction.course_code} · {message.pendingAction.course_name}</p>
                  <p>Semester: {message.pendingAction.semester}</p>
                  <p>Expires in about {Math.ceil(message.pendingAction.expires_in_seconds / 60)} minutes.</p>
                  {message.actionError && <p className="form-error" role="alert">{message.actionError}</p>}
                  {message.actionState === 'confirmed' && <p>Enrollment confirmed. My Plan has been refreshed.</p>}
                  {message.actionState === 'cancelled' && <p>Proposal cancelled. No enrollment was created.</p>}
                  {(message.actionState === 'pending' || message.actionState === 'failed') && (
                    <div className="actions">
                      <button type="button" onClick={() => confirm(message.id, message.pendingAction!.confirmation_id)}>
                        Confirm enrollment
                      </button>
                      <button type="button" className="secondary-button" onClick={() => updateAction(message.id, { actionState: 'cancelled', actionError: undefined })}>
                        Cancel
                      </button>
                    </div>
                  )}
                  {message.actionState === 'confirming' && <p>Confirming...</p>}
                </section>
              )}
            </article>
          ))}
        </section>
        <div className="message-field">
          <label htmlFor="message">Message</label>
          <textarea id="message" rows={4} value={draft} onChange={(event) => setDraft(event.target.value)} onKeyDown={keyDown} disabled={isSending} />
        </div>
        {formError && <p className="form-error" role="alert">{formError}</p>}
        <div className="actions">
          <button type="submit" disabled={isSending || !trimmed}>{isSending ? 'Sending...' : 'Send'}</button>
        </div>
      </form>
    </section>
  )
}
