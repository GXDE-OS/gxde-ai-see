// After DeepSeek Harness ui-chat message-chrome and use-calendar-day (MIT, see src/vendor/dsh/README.md).
import { useEffect, useState } from 'react'
import { t } from './strings'

const pad2 = (n: number) => String(n).padStart(2, '0')

function startOfDay(ms: number): number {
  return new Date(ms).setHours(0, 0, 0, 0)
}

function msUntilMidnight(ms: number): number {
  return Math.max(new Date(ms).setHours(24, 0, 0, 0) - ms, 1)
}

/** HH:mm today, with the date before it otherwise. */
export function formatClock(time: number, now: number): string {
  const d = new Date(time)
  const n = new Date(now)
  const clock = `${pad2(d.getHours())}:${pad2(d.getMinutes())}`
  if (startOfDay(time) === startOfDay(now)) return clock
  const date = d.getFullYear() === n.getFullYear()
    ? t.clockMd(d.getMonth() + 1, d.getDate())
    : t.clockYmd(d.getFullYear(), d.getMonth() + 1, d.getDate())
  return `${date} ${clock}`
}

export function formatDuration(ms: number): string {
  const total = Math.max(0, Math.floor(ms / 1000))
  const hours = Math.floor(total / 3600)
  const minutes = Math.floor(total / 60) % 60
  let text = ''
  if (hours > 0) text += `${hours}${t.hourUnit}`
  if (total >= 60) text += `${minutes}${t.minuteUnit}`
  return `${text}${total % 60}${t.secondUnit}`
}

/** Local midnight of today, advancing at each midnight. */
export function useCalendarDay(): number {
  const [day, setDay] = useState(() => startOfDay(Date.now()))
  useEffect(() => {
    const timer = { id: 0 }
    const arm = () => {
      const now = Date.now()
      setDay(startOfDay(now))
      timer.id = window.setTimeout(arm, msUntilMidnight(now))
    }
    timer.id = window.setTimeout(arm, msUntilMidnight(Date.now()))
    return () => {
      window.clearTimeout(timer.id)
    }
  }, [])
  return day
}
