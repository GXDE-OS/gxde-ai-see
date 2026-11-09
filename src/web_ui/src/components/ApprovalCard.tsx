/*
 * Behaviour from DeepSeek Harness ui-approval ApprovalPanel
 * (MIT, see src/vendor/dsh/README.md); looks like a DTK2 DDialog.
 */
import { useEffect, useRef, useState } from 'react'
import type { KeyboardEvent } from 'react'
import { t } from '../chat/strings'
import type { Approval } from '../chat/useChat'
import type { PermissionOption } from '../ipc/bridge'
import clsx from 'clsx'
import appIcon from '../assets/app-icon.png'
import css from '../vendor/dsh/approval/ApprovalPanel.module.css'
import local from './ApprovalCard.module.css'

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
      <div className={local.card} role="alertdialog" aria-label={t.approvalWaiting}>
        <div ref={body} className={local.top} tabIndex={0} role="group" aria-label={t.approvalDetail}>
          <img className={local.icon} src={appIcon} alt="" />
          <div className={local.text}>
            <div className={local.title}>{t.approvalTitle(name)}</div>
            {detail !== '' && <div className={local.message}>{detail}</div>}
          </div>
        </div>
        <div className={local.actions}>
          {options.map(option => (
            <button
              key={option.optionId}
              type="button"
              className={clsx(local.action, option.kind === 'allow_once' && local.recommend, option.kind.startsWith('reject') && local.warning)}
              disabled={answered}
              onClick={() => {
                answer(option.optionId)
              }}
            >
              {LABELS[option.kind]}
            </button>
          ))}
        </div>
      </div>
    </div>
  )
}
