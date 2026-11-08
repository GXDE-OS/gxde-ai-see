/*
 * Heavily referenced DeepSeek Harness ui-chat RunningStatus and
 * MessageItem notices, and ui-tool ToolRow (MIT, see src/vendor/dsh/README.md).
 */
import { useCallback, useEffect, useState } from 'react'
import type { ReactNode } from 'react'
import clsx from 'clsx'
import editIcon from '../assets/icons/edit.svg'
import fileReadIcon from '../assets/icons/file_read.svg'
import searchIcon from '../assets/icons/search.svg'
import thinkIcon from '../assets/icons/think_content.svg'
import { formatDuration } from '../chat/clock'
import { t } from '../chat/strings'
import type { ReplyPart, StopReason, ToolOutput } from '../ipc/bridge'
import { DisclosureRow } from '../vendor/dsh/primitives/DisclosureRow.tsx'
import {
  IconAgentPresetOutlineRegular, IconApiOutlineRegular, IconGlobeOutlineRegular, IconTrashOutlineRegular,
} from '../vendor/dsh/primitives/icons/index.tsx'
import { StateDot } from '../vendor/dsh/primitives/StateDot.tsx'
import { TextShimmer } from '../vendor/dsh/primitives/TextShimmer.tsx'
import a11y from '../vendor/dsh/chat/accessibility.module.css'
import cv from '../vendor/dsh/chat/ChatView.module.css'
import mi from '../vendor/dsh/chat/MessageItem.module.css'
import tr from '../vendor/dsh/tool/ToolRow.module.css'
import { MaskIcon } from './MaskIcon'
import css from './ReplyRows.module.css'

type ToolState = 'preparing' | 'running' | 'ok' | 'error' | 'stopped'

/** ACP tool kinds onto upstream's row variants and icons. */
function toolLook(kind: string): { variant: string; icon: ReactNode } {
  switch (kind) {
    case 'read': return { variant: 'read', icon: <MaskIcon src={fileReadIcon} size={14} /> }
    case 'search': return { variant: 'search', icon: <MaskIcon src={searchIcon} size={14} /> }
    case 'execute': return { variant: 'bash', icon: <IconApiOutlineRegular size={14} /> }
    case 'edit':
    case 'move': return { variant: 'edit', icon: <MaskIcon src={editIcon} size={14} /> }
    case 'delete': return { variant: 'edit', icon: <IconTrashOutlineRegular size={14} /> }
    case 'fetch': return { variant: 'others', icon: <IconGlobeOutlineRegular size={14} /> }
    case 'think': return { variant: 'others', icon: <MaskIcon src={thinkIcon} size={14} /> }
    default: return { variant: 'others', icon: <IconAgentPresetOutlineRegular size={14} /> }
  }
}

function DiffLine({ line }: { line: string }) {
  const mark = line.startsWith('@@') ? 'hunk' : line[0] === '+' ? 'added' : line[0] === '-' ? 'removed' : undefined
  return <div className={css.diffLine} data-diff={mark}>{line || ' '}</div>
}

function ToolOutputView({ output }: { output: ToolOutput[] }) {
  return (
    <div className={css.output}>
      {output.map((block, at) => block.type === 'text'
        ? <pre key={at} className={css.outputText}>{block.text}</pre>
        : (
            <div key={at} className={css.diff}>
              <div className={css.diffPath}>{block.path}</div>
              <pre className={css.diffBody}>{block.lines.map((line, index) => <DiffLine key={index} line={line} />)}</pre>
            </div>
          ))}
    </div>
  )
}

export function ToolRow({ part, turnRunning }: { part: Extract<ReplyPart, { type: 'tool' }>; turnRunning: boolean }) {
  const settled = part.status === 'completed' || part.status === 'failed'
  const state: ToolState = !settled && !turnRunning ? 'stopped'
    : part.status === 'pending' ? 'preparing'
      : part.status === 'in_progress' ? 'running'
        : part.status === 'failed' ? 'error' : 'ok'
  const summary = { preparing: t.toolPending, running: t.toolRunning, error: t.toolFailed, stopped: t.stopped, ok: '' }[state]
  const status = { preparing: t.toolPending, running: t.rowRunning, error: t.rowFailed, stopped: t.stopped, ok: null }[state]
  const { variant, icon } = toolLook(part.kind)
  const [open, setOpen] = useState(false)
  const toggle = useCallback(() => {
    setOpen(value => !value)
  }, [])
  const expandable = (part.output?.length ?? 0) > 0
  const collapsedContent = summary !== '' && (
    <>
      <span className={tr.sep} data-shimmer-decoration aria-hidden />
      <span className={clsx(tr.summary, state === 'error' && tr.errorSummary, state === 'stopped' && tr.stoppedSummary)}>
        <TextShimmer>{summary}</TextShimmer>
      </span>
    </>
  )
  return (
    <div className={tr.root} data-variant={variant} data-state={state}>
      {status !== null && <span className={tr.visuallyHidden}>{status}</span>}
      <DisclosureRow
        rowClassName={tr.row}
        leadingClassName={tr.leading}
        titleClassName={tr.title}
        icon={icon}
        title={part.title || part.kind}
        running={state === 'running' || state === 'preparing'}
        open={open && expandable}
        expandable={expandable}
        expandOnRowClick
        onToggle={toggle}
        collapsedContent={collapsedContent}
      >
        {part.output !== undefined && <ToolOutputView output={part.output} />}
      </DisclosureRow>
    </div>
  )
}

export function ReplyNotice({ stopReason, error }: { stopReason?: StopReason | undefined; error?: string | undefined }) {
  if (error !== undefined) {
    return (
      <div className={mi.turnErrorRow} role="status">
        <StateDot state="error" className={mi.turnErrorDot} />
        <div className={mi.turnErrorCopy}>
          <span className={mi.turnErrorTitle}>{t.turnError}</span>
          <span className={mi.turnErrorMessage}>{error}</span>
        </div>
      </div>
    )
  }
  const [title, hint] = stopReason === 'max_tokens' ? [t.maxTokens, t.maxTokensHint]
    : stopReason === 'max_turn_requests' ? [t.maxTurns, undefined]
      : stopReason === 'refusal' ? [t.refusal, undefined]
        : [t.stopped, undefined]
  return (
    <div className={mi.turnErrorRow} role="status">
      <StateDot state={stopReason === 'cancelled' ? 'idle' : 'warning'} className={mi.turnErrorDot} />
      <div className={mi.turnErrorCopy}>
        <span className={stopReason === 'cancelled' ? mi.turnErrorMessage : mi.maxTokensTitle}>{title}</span>
        {hint !== undefined && <span className={mi.turnErrorMessage}>{hint}</span>}
      </div>
    </div>
  )
}

// The native window's thresholds: AI Core retries failed requests silently.
const WAIT_NOTICE_MS = 20_000
const WAIT_HINT_MS = 45_000

export function RunningStatus({ startedAt, activityAt }: { startedAt: number | undefined; activityAt: number }) {
  const [now, setNow] = useState(Date.now)
  useEffect(() => {
    const timer = setInterval(() => {
      setNow(Date.now())
    }, 1000)
    return () => {
      clearInterval(timer)
    }
  }, [])
  const idle = now - activityAt
  const label = idle >= WAIT_NOTICE_MS ? t.waiting(Math.floor(idle / 1000))
    : startedAt === undefined ? t.working : t.workingFor(formatDuration(Math.max(1000, now - startedAt)))
  return (
    <div className={cv.running} data-chat-running>
      <span className={a11y.visuallyHidden} role="status" aria-live="polite" aria-atomic="true">{t.working}</span>
      <span className={cv.runningDivider} aria-hidden="true" />
      <span className={cv.runningContent}>
        <span className={cv.runningIcon} aria-hidden="true"><StateDot state="ongoing" /></span>
        <TextShimmer active className={cv.runningText}>{label}</TextShimmer>
      </span>
      {idle >= WAIT_HINT_MS && <span className={cv.hint}>{t.waitingHint}</span>}
    </div>
  )
}
