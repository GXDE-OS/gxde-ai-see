/*
 * Markup follows DeepSeek Harness ui-sidebar SidebarRoot and ui-workspace
 * session rows and rename dialog (MIT, see src/vendor/dsh/README.md).
 */
import { useEffect, useRef, useState } from 'react'
import clsx from 'clsx'
import appIcon from '../assets/app-icon.png'
import { t } from '../chat/strings'
import type { Conversation } from '../ipc/bridge'
import type { MenuEntry } from '../vendor/dsh/primitives/Menu.tsx'
import { Button } from '../vendor/dsh/primitives/Button.tsx'
import { HoverCard } from '../vendor/dsh/primitives/HoverCard.tsx'
import { HoverCardGroup } from '../vendor/dsh/primitives/HoverCardGroup.tsx'
import {
  IconEditOutlineRegular, IconEllipsisOutlineMedium, IconEllipsisOutlineRegular, IconInfoOutlineRegular, IconNewChatOutlineMedium,
  IconQueueOutlineRegular, IconRefreshOutlineRegular, IconSettingsOutlineMedium, IconStopFillRegular, IconTrashOutlineRegular,
} from '../vendor/dsh/primitives/icons/index.tsx'
import { Menu, MenuItemButton } from '../vendor/dsh/primitives/Menu.tsx'
import { Modal } from '../vendor/dsh/primitives/Modal.tsx'
import { StateDot } from '../vendor/dsh/primitives/StateDot.tsx'
import footer from '../vendor/dsh/settings/AccountMenu.module.css'
import sb from '../vendor/dsh/sidebar/SidebarRoot.module.css'
import wb from '../vendor/dsh/workspace/WorkspaceBrowser.module.css'
import rows from '../vendor/dsh/workspace/Rows.module.css'
import local from './Sidebar.module.css'

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

function preview(conversation: Conversation): string {
  const last = conversation.messages.findLast(message => message.content.trim() !== '')
  return last === undefined ? '' : last.content.replace(/\s+/g, ' ').trim().slice(0, 120)
}

type Modifiers = Pick<MouseEvent, 'ctrlKey' | 'metaKey' | 'shiftKey'>

interface SessionRowProps {
  session: Conversation
  now: number
  selected: boolean
  state: 'ongoing' | 'warning' | null
  deleteCount: number
  onClick: (modifiers: Modifiers) => void
  onRename: () => void
  onDelete: () => void
}

function SessionRow({ session, now, selected, state, deleteCount, onClick, onRename, onDelete }: SessionRowProps) {
  const [menuOpen, setMenuOpen] = useState(false)
  const title = session.title || t.untitled
  const text = preview(session)
  const span = timeLabel(session.updatedAt, now)
  const row = (
    <div
      className={clsx(rows.sessionRow, selected && rows.selected, menuOpen && rows.menuOpen)}
      role="treeitem"
      aria-selected={selected}
      tabIndex={0}
      data-session-id={session.id}
      onMouseDown={(event) => {
        if (event.shiftKey || event.ctrlKey) event.preventDefault()
      }}

      onClick={onClick}
      onContextMenu={(event) => {
        event.preventDefault()
        setMenuOpen(true)
      }}

      onKeyDown={(event) => {
        if (event.target !== event.currentTarget) return
        if (event.key === 'Enter') onClick(event)
        else if (event.key === 'Delete') onDelete()
        else if (event.key === 'F2') onRename()
      }}
    >
      <span className={rows.slot}>{state !== null && <StateDot state={state} />}</span>
      <span
        className={rows.title}
        onDoubleClick={(event) => {
          event.stopPropagation()
          onRename()
        }}
      >
        {title}
      </span>
      <span className={rows.time}>{span}</span>
      <span
        className={rows.rowActions}
        onClick={(event) => {
          event.stopPropagation()
        }}
      >
        <Menu
          open={menuOpen}
          onClose={() => {
            setMenuOpen(false)
          }}
          portal
          closeOnPointerLeave
          anchor={(
            <button
              type="button"
              className={rows.iconButton}
              aria-label={t.sessionActions(title)}
              onClick={() => {
                setMenuOpen(open => !open)
              }}
            >
              <IconEllipsisOutlineRegular />
            </button>
          )}
        >
          {deleteCount === 1 && (
            <MenuItemButton
              icon={<IconEditOutlineRegular />}
              onSelect={() => {
                setMenuOpen(false)
                onRename()
              }}
            >
              {t.rename}
            </MenuItemButton>
          )}
          <MenuItemButton
            icon={<IconTrashOutlineRegular />}
            danger
            onSelect={() => {
              setMenuOpen(false)
              onDelete()
            }}
          >
            {deleteCount === 1 ? t.delete : t.deleteSelected(deleteCount)}
          </MenuItemButton>
        </Menu>
      </span>
    </div>
  )

  return (
    <HoverCard
      anchor={row}
      content={(
        <div className={rows.hoverContent}>
          <div className={rows.hoverTitle}>{title}</div>
          <div className={rows.hoverTime}>{span === t.timeNow ? span : t.ago(span)}</div>
          {text !== '' && <div className={local.hoverPreview}>{text}</div>}
        </div>
      )}
      openDelayMs={800}
      grouped
      disabled={menuOpen}
      copyText={title}
      copyLabel={t.copy}
      copiedLabel={t.copied}
    />
  )
}

function RenameDialog({ title, onCancel, onConfirm }: { title: string; onCancel: () => void; onConfirm: (title: string) => void }) {
  const [draft, setDraft] = useState(title)
  const composing = useRef(false)
  const trimmed = draft.trim()
  const confirm = () => {
    if (trimmed !== '') onConfirm(trimmed)
  }
  return (
    <Modal
      open
      onClose={onCancel}
      closeLabel={t.close}
      title={t.renameSession}
      footer={(
        <>
          <Button variant="outline" onClick={onCancel}>{t.cancel}</Button>
          <Button variant="primary" disabled={trimmed === ''} onClick={confirm}>{t.rename}</Button>
        </>
      )}
    >
      <input
        className={wb.renameInput}
        value={draft}
        aria-label={t.sessionName}
        data-modal-autofocus
        onFocus={(event) => {
          event.target.select()
        }}
        onChange={(event) => {
          setDraft(event.target.value)
        }}
        onCompositionStart={() => {
          composing.current = true
        }}
        onCompositionEnd={() => {
          composing.current = false
        }}
        onKeyDown={(event) => {
          if (event.key === 'Enter' && !composing.current) {
            event.preventDefault()
            confirm()
          }
        }}
      />
    </Modal>
  )
}

export interface SidebarProps {
  sessions: Conversation[]
  currentId: string | null
  onNew: () => void
  onOpen: (id: string) => void
  onRename: (id: string, title: string) => void
  onDelete: (ids: string[]) => void
  runningId: string | null
  /** Conversations waiting on a tool confirmation. */
  waitingIds: string[]
  actions: FooterActions
}

export interface FooterActions {
  onSettings: (() => void) | undefined
  onStop: (() => void) | undefined
  onRetryLast: (() => void) | undefined
  onRestartCore: (() => void) | undefined
  onAbout: (() => void) | undefined
}

function Footer({ onSettings, onStop, onRetryLast, onRestartCore, onAbout }: FooterActions) {
  const [open, setOpen] = useState(false)
  const run: Record<string, (() => void) | undefined> = { stop: onStop, retry: onRetryLast, restart: onRestartCore, about: onAbout }
  const items: MenuEntry[] = [
    { id: 'stop', label: t.stopReply, icon: <IconStopFillRegular size={16} />, disabled: onStop === undefined },
    { id: 'retry', label: t.retryLast, icon: <IconRefreshOutlineRegular size={16} />, disabled: onRetryLast === undefined },
    { type: 'separator', id: 'core' },
    { id: 'restart', label: t.restartCore, icon: <IconRefreshOutlineRegular size={16} />, disabled: onRestartCore === undefined },
    { id: 'about', label: t.about, icon: <IconInfoOutlineRegular size={16} />, disabled: onAbout === undefined },
  ]
  return (
    <div className={sb.footArea}>
      <div className={clsx(sb.settingsArea, local.footer)}>
        <button type="button" className={clsx(footer.trigger, local.footerButton)} disabled={onSettings === undefined} onClick={onSettings}>
          <IconSettingsOutlineMedium size={16} />
          <span className={footer.label}>{t.settings}</span>
        </button>
        <Menu
          open={open}
          items={items}
          side="top"
          align="end"
          portal
          onSelect={(id) => {
            setOpen(false)
            run[id]?.()
          }}
          onClose={() => {
            setOpen(false)
          }}
          anchor={(
            <button
              type="button"
              className={clsx(footer.trigger, local.footerButton)}
              data-collapsed="true"
              aria-label={t.more}
              aria-haspopup="menu"
              aria-expanded={open}
              onClick={() => {
                setOpen(!open)
              }}
            >
              <IconEllipsisOutlineMedium size={14} />
            </button>
          )}
        />
      </div>
    </div>
  )
}

export function Sidebar({ sessions, currentId, onNew, onOpen, onRename, onDelete, runningId, waitingIds, actions }: SidebarProps) {
  const now = useNow(60_000)
  const [picked, setPicked] = useState<string[]>([])
  const rangeStart = useRef<string | null>(null)
  const [renaming, setRenaming] = useState<Conversation | null>(null)

  const alive = picked.filter(id => sessions.some(session => session.id === id))
  const selection = alive.length > 0 ? alive : currentId === null ? [] : [currentId]

  const onRowClick = (id: string, event: Modifiers) => {
    if (event.ctrlKey || event.metaKey) {
      setPicked(selection.includes(id) ? selection.filter(other => other !== id) : [...selection, id])
      rangeStart.current = id
      return
    }

    const start = sessions.findIndex(session => session.id === (rangeStart.current ?? currentId))
    if (event.shiftKey && start !== -1) {
      const end = sessions.findIndex(session => session.id === id)
      setPicked(sessions.slice(Math.min(start, end), Math.max(start, end) + 1).map(session => session.id))
      return
    }

    setPicked([])
    rangeStart.current = id
    onOpen(id)
  }

  const deletion = (id: string) => selection.length > 1 && selection.includes(id) ? selection : [id]

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
          <div className={wb.listArea} data-drop-new="">
            <HoverCardGroup className={clsx(wb.treeBody, wb.wide)}>
              <div className={clsx(wb.list, wb.flatList)} role="tree" aria-label={t.sessions}>
                {sessions.length === 0 && (
                  <div className={wb.emptyState}>
                    <IconQueueOutlineRegular size={24} />
                    <div>{t.noSessions}</div>
                  </div>
                )}
                {sessions.map(session => (
                  <SessionRow
                    key={session.id}
                    session={session}
                    now={now}
                    selected={selection.includes(session.id)}
                    state={waitingIds.includes(session.id) ? 'warning' : session.id === runningId ? 'ongoing' : null}
                    deleteCount={deletion(session.id).length}
                    onClick={(event) => {
                      onRowClick(session.id, event)
                    }}
                    onRename={() => {
                      setRenaming(session)
                    }}
                    onDelete={() => {
                      onDelete(deletion(session.id))
                      setPicked([])
                    }}
                  />
                ))}
              </div>
            </HoverCardGroup>
          </div>
        </div>
      </div>
      <Footer {...actions} />
      {renaming !== null && (
        <RenameDialog
          key={renaming.id}
          title={renaming.title}
          onCancel={() => {
            setRenaming(null)
          }}
          onConfirm={(title) => {
            onRename(renaming.id, title)
            setRenaming(null)
          }}
        />
      )}
    </div>
  )
}
