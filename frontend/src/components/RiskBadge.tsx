import type { RiskLevel } from '../types/student'

export default function RiskBadge({ level }: { level: RiskLevel }) {
  return <span className={`badge badge-risk-${level.toLowerCase()}`}>{level} risk</span>
}
