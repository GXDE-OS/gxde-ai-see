/*
 * Markup follows DeepSeek Harness ui-sidebar SidebarRoot and ui-workspace
 * session rows and rename dialog (MIT, see src/vendor/dsh/README.md).
 */
import { useEffect, useRef, useState } from 'react'
import clsx from 'clsx'
import newDialogueIcon from '../assets/icons/new_dialogue.svg'
import settingsIcon from '../assets/icons/settings.svg'
import { t } from '../chat/strings'
import type { Conversation } from '../ipc/bridge'
import { Button } from '../vendor/dsh/primitives/Button.tsx'
import { HoverCard } from '../vendor/dsh/primitives/HoverCard.tsx'
import { HoverCardGroup } from '../vendor/dsh/primitives/HoverCardGroup.tsx'
import {
  IconEditOutlineRegular, IconEllipsisOutlineRegular, IconQueueOutlineRegular, IconTrashOutlineRegular,
} from '../vendor/dsh/primitives/icons/index.tsx'
import { Menu, MenuItemButton } from '../vendor/dsh/primitives/Menu.tsx'
import { Modal } from '../vendor/dsh/primitives/Modal.tsx'
import { StateDot } from '../vendor/dsh/primitives/StateDot.tsx'
import footer from '../vendor/dsh/settings/AccountMenu.module.css'
import sb from '../vendor/dsh/sidebar/SidebarRoot.module.css'
import wb from '../vendor/dsh/workspace/WorkspaceBrowser.module.css'
import rows from '../vendor/dsh/workspace/Rows.module.css'
import { MaskIcon } from './MaskIcon'
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
      className={clsx(rows.sessionRow, local.row, selected && [rows.selected, local.selected], menuOpen && rows.menuOpen)}
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
        className={clsx(rows.title, local.rowTitle)}
        onDoubleClick={(event) => {
          event.stopPropagation()
          onRename()
        }}
      >
        {title}
      </span>
      <span className={clsx(rows.time, local.rowTime)}>{span}</span>
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
  onSettings: (() => void) | undefined
}

function Footer({ onSettings }: { onSettings: (() => void) | undefined }) {
  return (
    <div className={sb.footArea}>
      <div className={clsx(sb.settingsArea, local.footer)}>
        <button type="button" className={clsx(footer.trigger, local.footerButton)} disabled={onSettings === undefined} onClick={onSettings}>
          <MaskIcon src={settingsIcon} size={16} />
          <span className={footer.label}>{t.settings}</span>
        </button>
      </div>
    </div>
  )
}

export function Sidebar({ sessions, currentId, onNew, onOpen, onRename, onDelete, runningId, waitingIds, onSettings }: SidebarProps) {
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
    <div className={clsx(sb.root, local.root)}>
      <button type="button" className={clsx(sb.newSession, local.newSession)} onClick={onNew}>
        <span className={sb.newSessionLabelMask}>
          <span className={clsx(sb.newSessionContent, local.newSessionContent)}>
            <MaskIcon src={newDialogueIcon} size={18} />
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
      <Footer onSettings={onSettings} />
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
