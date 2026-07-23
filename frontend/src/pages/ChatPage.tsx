import type { FormEvent, KeyboardEvent } from 'react'
import { useMemo, useState } from 'react'
import { useNavigate } from 'react-router-dom'
import { useAuth } from '../auth/AuthContext'
import { queryAgent } from '../services/agentApi'
import { isAuthError } from '../services/api'
import EmptyState from '../components/EmptyState'

type ChatMessage = {
  id: number
  role: 'assistant' | 'user'
  content: string
  status?: string
  toolsUsed?: string[]
}

const fallbackError = 'Unable to contact the advisor. Please try again.'
const initialAssistantMessage =
  'Hello! How can I help with your academic planning?'

export default function ChatPage() {
  const { user, token, logout } = useAuth()
  const navigate = useNavigate()
  const studentId = user?.student_id ?? null

  const [draftMessage, setDraftMessage] = useState('')
  const [messages, setMessages] = useState<ChatMessage[]>([
    {
      id: 1,
      role: 'assistant',
      content: initialAssistantMessage,
    },
  ])
  const [isSending, setIsSending] = useState(false)
  const [formError, setFormError] = useState('')

  const trimmedDraft = draftMessage.trim()
  const isMessageEmpty = trimmedDraft.length === 0
  const nextMessageId = useMemo(
    () => Math.max(...messages.map((message) => message.id)) + 1,
    [messages],
  )

  async function handleSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault()

    if (studentId === null) return
    if (isMessageEmpty) {
      setFormError('Enter a message before sending.')
      return
    }

    setIsSending(true)
    setFormError('')

    try {
      const responseBody = await queryAgent(studentId, trimmedDraft, token ?? '')

      setMessages((currentMessages) => [
        ...currentMessages,
        {
          id: nextMessageId,
          role: 'user',
          content: trimmedDraft,
        },
        {
          id: nextMessageId + 1,
          role: 'assistant',
          content: responseBody.answer ?? fallbackError,
          status: responseBody.status,
          toolsUsed: responseBody.tools_used,
        },
      ])
      setDraftMessage('')
    } catch (error) {
      if (isAuthError(error)) {
        logout()
        navigate('/login', { replace: true })
        return
      }
      setFormError(error instanceof Error ? error.message : fallbackError)
    } finally {
      setIsSending(false)
    }
  }

  function handleTextareaKeyDown(event: KeyboardEvent<HTMLTextAreaElement>) {
    if (event.key === 'Enter' && !event.shiftKey) {
      event.preventDefault()
      event.currentTarget.form?.requestSubmit()
    }
  }

  if (studentId === null) {
    return (
      <section className="chat-panel" aria-labelledby="page-title">
        <header className="chat-header">
          <h1 id="page-title">AI Advisor</h1>
        </header>
        <EmptyState message="Your account isn't linked to a student profile, so the advisor chat isn't available. Contact your advisor if you believe this is a mistake." />
      </section>
    )
  }

  return (
    <section className="chat-panel" aria-labelledby="page-title">
      <header className="chat-header">
        <h1 id="page-title">AI Advisor</h1>
        <p>
          Ask questions about courses, semester planning, recommendations, and
          academic risk.
        </p>
      </header>

      <form className="chat-form" onSubmit={handleSubmit}>
        <section
          className="conversation"
          aria-label="Conversation with advisor"
        >
          {messages.map((message) => (
            <article
              className={`message message-${message.role}`}
              key={message.id}
            >
              <p className="message-label">
                {message.role === 'assistant' ? 'Advisor' : 'You'}
              </p>
              <p className="message-content">{message.content}</p>
              {message.status && (
                <p className="message-status">Status: {message.status}</p>
              )}
              {message.toolsUsed && message.toolsUsed.length > 0 && (
                <div className="tools-used">
                  <p>Tools used</p>
                  <ul>
                    {message.toolsUsed.map((toolName) => (
                      <li key={toolName}>{toolName}</li>
                    ))}
                  </ul>
                </div>
              )}
            </article>
          ))}
        </section>

        <div className="message-field">
          <label htmlFor="message">Message</label>
          <textarea
            id="message"
            name="message"
            rows={4}
            value={draftMessage}
            onChange={(event) => setDraftMessage(event.target.value)}
            onKeyDown={handleTextareaKeyDown}
            placeholder="Build me a light semester plan"
          />
        </div>

        {formError && (
          <p className="form-error" role="alert">
            {formError}
          </p>
        )}

        <div className="actions">
          <button type="submit" disabled={isSending || isMessageEmpty}>
            {isSending ? 'Sending...' : 'Send'}
          </button>
        </div>
      </form>
    </section>
  )
}
