import { createContext, useContext, useMemo, useState } from 'react'
import type { Dispatch, ReactNode, SetStateAction } from 'react'

export type ChatMessage = {
  id: number
  role: 'assistant' | 'user'
  content: string
  status?: string
  toolsUsed?: string[]
}

const ChatContext = createContext<{
  messages: ChatMessage[]
  setMessages: Dispatch<SetStateAction<ChatMessage[]>>
  resetChat: () => void
} | null>(null)

const initialMessages: ChatMessage[] = [{
  id: 1,
  role: 'assistant',
  content: 'I provide read-only recommendations and semester-plan previews. To add or remove an official planned course, use the My Plan page.',
}]

export function ChatProvider({ children }: { children: ReactNode }) {
  const [messages, setMessages] = useState<ChatMessage[]>(initialMessages)
  const value = useMemo(
    () => ({ messages, setMessages, resetChat: () => setMessages(initialMessages) }),
    [messages],
  )
  return <ChatContext.Provider value={value}>{children}</ChatContext.Provider>
}

export function useChat() {
  const context = useContext(ChatContext)
  if (!context) throw new Error('useChat must be used within ChatProvider')
  return context
}
