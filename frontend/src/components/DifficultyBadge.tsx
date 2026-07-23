import type { DifficultyLevel } from '../types/course'

export default function DifficultyBadge({ level }: { level: DifficultyLevel }) {
  return <span className={`badge badge-difficulty-${level}`}>{level}</span>
}
