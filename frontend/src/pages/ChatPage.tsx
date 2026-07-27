import type { FormEvent, KeyboardEvent } from 'react'
import { useEffect, useMemo, useRef, useState } from 'react'
import { useAuth } from '../auth/AuthContext'
import { queryAgent } from '../services/agentApi'
import { useChat } from '../chat/ChatContext'
import EmptyState from '../components/EmptyState'

const fallbackError = 'Unable to contact the advisor. Please try again.'

export default function ChatPage() {
  const { user } = useAuth()
  const { messages, setMessages } = useChat()
  const [draft, setDraft] = useState('')
  const [isSending, setIsSending] = useState(false)
  const [formError, setFormError] = useState('')
  const endRef = useRef<HTMLDivElement>(null)
  const trimmed = draft.trim()
  const nextId = useMemo(() => Math.max(...messages.map((item) => item.id)) + 1, [messages])

  useEffect(() => {
    endRef.current?.scrollIntoView({ behavior: 'smooth', block: 'end' })
  }, [messages, formError, isSending])

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
      }])
    } catch (error) {
      const message = error instanceof Error ? error.message : fallbackError
      setFormError(message)
      setMessages((current) => [...current, {
        id: nextId + 1,
        role: 'assistant',
        content: message,
        status: 'error',
      }])
    } finally {
      setIsSending(false)
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
        <p>
          Get read-only recommendations and semester-plan previews. Add or remove
          official planned courses yourself from My Plan.
        </p>
      </header>
      <form className="chat-form" onSubmit={submit}>
        <section className="conversation" aria-label="Conversation with advisor">
          {messages.map((message) => (
            <article className={`message message-${message.role}`} key={message.id}>
              <p className="message-label">{message.role === 'assistant' ? 'Advisor' : 'You'}</p>
              <p className="message-content">{message.content}</p>
              {message.toolsUsed?.length ? <p className="message-status">Tools: {message.toolsUsed.join(', ')}</p> : null}
            </article>
          ))}
          <div ref={endRef} aria-hidden="true" />
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
