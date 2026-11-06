/*
 * Heavily referenced DeepSeek Harness ui-approval ApprovalPanel
 * (MIT, see src/vendor/dsh/README.md).
 */
import { useEffect, useRef, useState } from 'react'
import type { KeyboardEvent } from 'react'
import { t } from '../chat/strings'
import type { Approval } from '../chat/useChat'
import type { PermissionOption } from '../ipc/bridge'
import { Button } from '../vendor/dsh/primitives/Button.tsx'
import { StateDot } from '../vendor/dsh/primitives/StateDot.tsx'
import css from '../vendor/dsh/approval/ApprovalPanel.module.css'

const DETAIL_LIMIT = 600

/** What the tool is about to do: its text content, diffs and paths. */
function detailOf(call: Approval['toolCall']): string {
  const lines: string[] = []
  for (const item of (call.content ?? []) as { type?: string; content?: { text?: string }; path?: string }[]) {
    if (item.type === 'content' && item.content?.text?.trim()) lines.push(item.content.text.trim())
    else if (item.type === 'diff' && item.path !== undefined) lines.push(t.editFile(item.path))
  }
  for (const { path } of call.locations ?? []) {
    if (path !== '' && !lines.some(line => line.includes(path))) lines.push(path)
  }
  const detail = lines.join('\n')
  return detail.length > DETAIL_LIMIT ? `${detail.slice(0, DETAIL_LIMIT)}…` : detail
}

const LABELS: Record<PermissionOption['kind'], string> = {
  allow_once: t.allowOnce,
  allow_always: t.allowAlways,
  reject_once: t.rejectOnce,
  reject_always: t.rejectAlways,
}

// Rejections first, so the primary allow ends the row.
const ORDER: PermissionOption['kind'][] = ['reject_always', 'reject_once', 'allow_always', 'allow_once']

export function ApprovalCard({ approval, onAnswer }: { approval: Approval; onAnswer: (optionId: string) => void }) {
  const [answered, setAnswered] = useState(false)
  const body = useRef<HTMLDivElement>(null)
  // Enter allows and Escape rejects straight away, as upstream's panel does.
  useEffect(() => {
    body.current?.focus({ preventScroll: true })
  }, [])
  const name = (approval.toolCall.title ?? '').trim() || approval.toolCall.kind || ''
  const detail = detailOf(approval.toolCall)
  const options = ORDER.flatMap(kind => approval.options.filter(option => option.kind === kind))
  const answer = (optionId: string | undefined) => {
    if (answered || optionId === undefined) return
    setAnswered(true)
    onAnswer(optionId)
  }
  const keydown = (event: KeyboardEvent<HTMLDivElement>) => {
    if (event.nativeEvent.isComposing || event.ctrlKey || event.metaKey || event.altKey || event.shiftKey) return
    if ((event.target as Element).closest('button') !== null && event.key === 'Enter') return
    if (event.key === 'Enter') answer(options.find(option => option.kind === 'allow_once')?.optionId)
    else if (event.key === 'Escape') answer(options.find(option => option.kind === 'reject_once')?.optionId)
    else return
    event.preventDefault()
  }
  return (
    <div className={css.root} aria-busy={answered} onKeyDown={keydown}>
      <div className={css.card}>
        <div className={css.strip}><StateDot state={answered ? 'ongoing' : 'warning'} />{t.approvalWaiting}</div>
        <div ref={body} className={css.body} tabIndex={0} role="group" aria-label={t.approvalDetail}>
          <div className={css.headline}>{t.approvalTitle(name)}</div>
          {detail !== '' && <div className={css.command}>{detail}</div>}
        </div>
        <div className={css.actionRow}>
          {options.map(option => (
            <Button
              key={option.optionId}
              variant={option.kind === 'allow_once' ? 'primary' : 'outline'}
              className={option.kind.startsWith('reject') ? css.reject : undefined}
              disabled={answered}
              onClick={() => {
                answer(option.optionId)
              }}
            >
              {LABELS[option.kind]}
            </Button>
          ))}
        </div>
      </div>
    </div>
  )
}
