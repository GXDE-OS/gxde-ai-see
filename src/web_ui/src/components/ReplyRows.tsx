/*
 * Heavily referenced DeepSeek Harness ui-chat ReasoningRow, RunningStatus and
 * MessageItem notices, and ui-tool ToolRow (MIT, see src/vendor/dsh/README.md).
 */
import { useCallback, useEffect, useMemo, useState } from 'react'
import type { ReactNode } from 'react'
import clsx from 'clsx'
import { formatDuration } from '../chat/clock'
import { t } from '../chat/strings'
import type { ReplyPart, StopReason } from '../ipc/bridge'
import { DisclosureRow } from '../vendor/dsh/primitives/DisclosureRow.tsx'
import {
  IconAgentPresetOutlineRegular, IconApiOutlineRegular, IconBrowseOutlineRegular, IconEditOutlineRegular,
  IconGlobeOutlineRegular, IconSearchOutlineRegular, IconThinkOutlineRegular, IconTrashOutlineRegular,
} from '../vendor/dsh/primitives/icons/index.tsx'
import { MarkdownText } from '../vendor/dsh/primitives/markdown/MarkdownText.tsx'
import type { MarkdownLabels } from '../vendor/dsh/primitives/markdown/render.tsx'
import { StateDot } from '../vendor/dsh/primitives/StateDot.tsx'
import { TextShimmer } from '../vendor/dsh/primitives/TextShimmer.tsx'
import a11y from '../vendor/dsh/chat/accessibility.module.css'
import cv from '../vendor/dsh/chat/ChatView.module.css'
import mi from '../vendor/dsh/chat/MessageItem.module.css'
import rr from '../vendor/dsh/chat/ReasoningRow.module.css'
import tr from '../vendor/dsh/tool/ToolRow.module.css'

const THINK_ICON = <IconThinkOutlineRegular size={14} />

function firstLine(text: string): string {
  const newline = text.indexOf('\n')
  return newline === -1 ? text : text.slice(0, newline)
}

function latestCompletedParagraphFirstLine(text: string): string {
  let summary = ''
  let paragraphStart = 0
  const separator = /\r?\n(?:[\t ]*\r?\n)+/g
  for (;;) {
    const nextParagraph = separator.exec(text)
    const paragraphEnd = nextParagraph === null ? text.length : nextParagraph.index + nextParagraph[0].indexOf('\n')
    const newline = text.indexOf('\n', paragraphStart)
    if (newline !== -1 && newline <= paragraphEnd) {
      const candidate = text.slice(paragraphStart, newline).trim()
      if (candidate !== '') summary = candidate
    }
    if (nextParagraph === null) return summary
    paragraphStart = nextParagraph.index + nextParagraph[0].length
  }
}

export function ReasoningRow({ part, running, labels }: {
  part: Extract<ReplyPart, { type: 'reasoning' }>
  running: boolean
  labels: MarkdownLabels
}) {
  const [expanded, setExpanded] = useState(false)
  const toggle = useCallback(() => {
    setExpanded(open => !open)
  }, [])
  const summary = (running ? latestCompletedParagraphFirstLine(part.text) : firstLine(part.text)).replaceAll('**', '')
  const title = running
    ? t.thinking
    : part.endedAt === undefined ? t.think : t.thought(formatDuration(Math.max(1000, part.endedAt - part.startedAt)))
  const collapsedContent = useMemo(() => (
    <>
      <span className={rr.separator} data-shimmer-decoration aria-hidden />
      <span className={rr.summary} data-streaming={running || undefined}>
        <span className={rr.summaryText}>
          <TextShimmer>{summary}</TextShimmer>
        </span>
      </span>
    </>
  ), [running, summary])
  return (
    <div
      className={rr.root}
      data-variant="think"
      data-state={running ? 'running' : 'ok'}
      data-expanded={expanded || undefined}
      data-preview={(!expanded && running && summary !== '') || undefined}
    >
      {running && <span className={a11y.visuallyHidden}>{t.rowRunning}</span>}
      <DisclosureRow
        rowClassName={rr.row}
        leadingClassName={rr.leading}
        titleClassName={rr.title}
        icon={THINK_ICON}
        title={title}
        running={running}
        open={expanded}
        expandable
        expandOnRowClick
        onToggle={toggle}
        collapsedContent={collapsedContent}
      >
        {expanded && (
          <div className={rr.thinkBody}>
            <MarkdownText text={part.text} streaming={running} labels={labels} variant="compact" />
          </div>
        )}
      </DisclosureRow>
    </div>
  )
}

type ToolState = 'preparing' | 'running' | 'ok' | 'error' | 'stopped'

/** ACP tool kinds onto upstream's row variants and icons. */
function toolLook(kind: string): { variant: string; icon: ReactNode } {
  switch (kind) {
    case 'read': return { variant: 'read', icon: <IconBrowseOutlineRegular size={14} /> }
    case 'search': return { variant: 'search', icon: <IconSearchOutlineRegular size={14} /> }
    case 'execute': return { variant: 'bash', icon: <IconApiOutlineRegular size={14} /> }
    case 'edit':
    case 'move': return { variant: 'edit', icon: <IconEditOutlineRegular size={14} /> }
    case 'delete': return { variant: 'edit', icon: <IconTrashOutlineRegular size={14} /> }
    case 'fetch': return { variant: 'others', icon: <IconGlobeOutlineRegular size={14} /> }
    case 'think': return { variant: 'others', icon: THINK_ICON }
    default: return { variant: 'others', icon: <IconAgentPresetOutlineRegular size={14} /> }
  }
}

const noop = () => undefined

export function ToolRow({ part, turnRunning }: { part: Extract<ReplyPart, { type: 'tool' }>; turnRunning: boolean }) {
  const settled = part.status === 'completed' || part.status === 'failed'
  const state: ToolState = !settled && !turnRunning ? 'stopped'
    : part.status === 'pending' ? 'preparing'
      : part.status === 'in_progress' ? 'running'
        : part.status === 'failed' ? 'error' : 'ok'
  const summary = { preparing: t.toolPending, running: t.toolRunning, error: t.toolFailed, stopped: t.stopped, ok: '' }[state]
  const status = { preparing: t.toolPending, running: t.rowRunning, error: t.rowFailed, stopped: t.stopped, ok: null }[state]
  const { variant, icon } = toolLook(part.kind)
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
        open={false}
        expandable={false}
        onToggle={noop}
        collapsedContent={collapsedContent}
      />
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
