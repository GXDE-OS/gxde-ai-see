/*
 * Markup follows DeepSeek Harness ui-sidebar SidebarRoot and ui-workspace
 * session rows (MIT, see src/vendor/dsh/README.md).
 */
import { useEffect, useState } from 'react'
import clsx from 'clsx'
import appIcon from '../assets/app-icon.png'
import { t } from '../chat/strings'
import { IconNewChatOutlineMedium, IconQueueOutlineRegular } from '../vendor/dsh/primitives/icons/index.tsx'
import sb from '../vendor/dsh/sidebar/SidebarRoot.module.css'
import wb from '../vendor/dsh/workspace/WorkspaceBrowser.module.css'
import rows from '../vendor/dsh/workspace/Rows.module.css'
import local from './Sidebar.module.css'

export interface SessionSummary {
  id: string
  title: string
  updatedAt: number
}

function timeLabel(updatedAt: number, now: number): string {
  const minutes = Math.floor((now - updatedAt) / 60_000)
  if (minutes < 1) return t.timeNow
  if (minutes < 60) return t.minutes(minutes)
  const hours = Math.floor(minutes / 60)
  if (hours < 24) return t.hours(hours)
  const days = Math.floor(hours / 24)
  if (days < 30) return t.days(days)
  if (days < 365) return t.months(Math.floor(days / 30))
  return t.years(Math.floor(days / 365))
}

function useNow(intervalMs: number): number {
  const [now, setNow] = useState(Date.now)
  useEffect(() => {
    const timer = setInterval(() => {
      setNow(Date.now())
    }, intervalMs)
    return () => {
      clearInterval(timer)
    }
  }, [intervalMs])
  return now
}

export interface SidebarProps {
  sessions: SessionSummary[]
  currentId: string | null
  onNew: () => void
  onOpen: (id: string) => void
}

export function Sidebar({ sessions, currentId, onNew, onOpen }: SidebarProps) {
  const now = useNow(60_000)

  return (
    <div className={sb.root}>
      <div className={sb.logoRow}>
        <button type="button" className={clsx(sb.brand, sb.wide)} aria-label={t.newSession} onClick={onNew}>
          <span className={sb.brandIdentity} aria-hidden="true">
            <span className={sb.brandMark}>
              <img className={local.brandIcon} src={appIcon} alt="" />
            </span>
            <span className={sb.brandName}>
              <span className={sb.fallbackBrandName}>{t.appName}</span>
            </span>
          </span>
        </button>
      </div>

      <button type="button" className={sb.newSession} onClick={onNew}>
        <span className={sb.newSessionLabelMask}>
          <span className={sb.newSessionContent}>
            <IconNewChatOutlineMedium size={14} />
            <span className={clsx(sb.newSessionLabel, sb.wide)}>{t.newSession}</span>
          </span>
        </span>
      </button>

      <div className={sb.regionArea}>
        <div className={wb.root}>
          <div className={wb.sectionHeader}>
            <span className={clsx(wb.sectionLabel, wb.wide, local.sectionLabel)}>{t.sessions}</span>
          </div>
          <div className={wb.listArea}>
            <div className={clsx(wb.treeBody, wb.wide)}>
              <div className={clsx(wb.list, wb.flatList)} role="tree" aria-label={t.sessions}>
                {sessions.length === 0 && (
                  <div className={wb.emptyState}>
                    <IconQueueOutlineRegular size={24} />
                    <div>{t.noSessions}</div>
                  </div>
                )}
                {sessions.map(session => (
                  <div
                    key={session.id}
                    className={clsx(rows.sessionRow, session.id === currentId && rows.selected)}
                    role="treeitem"
                    aria-selected={session.id === currentId}
                    tabIndex={0}
                    onClick={() => {
                      onOpen(session.id)
                    }}
                    onKeyDown={(event) => {
                      if (event.key === 'Enter') onOpen(session.id)
                    }}
                  >
                    <span className={rows.slot} />
                    <span className={rows.title}>{session.title || t.untitled}</span>
                    <span className={rows.time}>{timeLabel(session.updatedAt, now)}</span>
                  </div>
                ))}
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  )
}
