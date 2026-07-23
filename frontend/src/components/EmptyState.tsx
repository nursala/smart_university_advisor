import type { ReactNode } from 'react'

export default function EmptyState({
  message,
  action,
}: {
  message: string
  action?: ReactNode
}) {
  return (
    <div className="state-panel state-empty">
      <p>{message}</p>
      {action}
    </div>
  )
}
