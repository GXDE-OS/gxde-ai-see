/*
 * Heavily referenced DeepSeek Harness ui-chat ChatView, UserContent,
 * AssistantMarkdown, AssistantNodeView, MessageIconActions and TurnTailNodeView
 * (MIT, see src/vendor/dsh/README.md).
 */
import { memo, useCallback, useEffect, useLayoutEffect, useMemo, useRef, useState } from 'react'
import type { ReactNode } from 'react'
import clsx from 'clsx'
import { formatClock, useCalendarDay } from '../chat/clock'
import { replyParts } from '../chat/reply'
import { t } from '../chat/strings'
import type { ChatMessage } from '../ipc/bridge'
import { writeClipboard } from '../vendor/dsh/primitives/clipboard.ts'
import { Button } from '../vendor/dsh/primitives/Button.tsx'
import {
  IconBranchOutlineRegular, IconCheckOutlineRegular, IconChevronDownOutlineRegular, IconCopyOutlineRegular, IconEditOutlineRegular,
  IconRefreshOutlineRegular,
} from '../vendor/dsh/primitives/icons/index.tsx'
import { MarkdownText } from '../vendor/dsh/primitives/markdown/MarkdownText.tsx'
import type { MarkdownLabels } from '../vendor/dsh/primitives/markdown/render.tsx'
import { Tooltip } from '../vendor/dsh/primitives/Tooltip.tsx'
import cv from '../vendor/dsh/chat/ChatView.module.css'
import mi from '../vendor/dsh/chat/MessageItem.module.css'
import am from '../vendor/dsh/chat/AssistantMarkdown.module.css'
import ma from '../vendor/dsh/chat/MessageIconActions.module.css'
import tt from '../vendor/dsh/chat/TurnTailNodeView.module.css'
import ie from '../vendor/dsh/primitives/InlineEditor.module.css'
import { ReasoningRow, ReplyNotice, RunningStatus, ToolRow } from './ReplyRows'
import local from './ChatView.module.css'

const LABELS: MarkdownLabels = {
  code: {
    copyLabel: t.copy,
    copiedLabel: t.copied,
    toolbarLabels: { codeLabel: t.codeBlock, wrapLabel: t.wrap, unwrapLabel: t.unwrap },
  },
  footnotes: t.footnotes,
}

const FOLLOW_SLACK = 32

interface Action {
  label: string
  icon: ReactNode
  run: () => void
}

function ActionButton({ label, icon, run }: Action) {
  return (
    <Tooltip label={label} side="bottom">
      <button type="button" className={ma.action} aria-label={label} onClick={run}>{icon}</button>
    </Tooltip>
  )
}

interface MessageActionsProps {
  text: string
  time: number | undefined
  clock: 'start' | 'end'
  actions: Action[]
  className?: string
}

function MessageActions({ text, time, clock, actions, className }: MessageActionsProps) {
  const day = useCalendarDay()
  const [copied, setCopied] = useState(false)
  const timer = useRef<number | undefined>(undefined)
  useEffect(() => () => {
    window.clearTimeout(timer.current)
  }, [])
  const onCopy = useCallback(() => {
    if (copied) return
    void writeClipboard(text).then((ok) => {
      if (!ok) return
      setCopied(true)
      timer.current = window.setTimeout(() => {
        setCopied(false)
      }, 1000)
    })
  }, [copied, text])
  const label = copied ? t.copied : t.copy
  const clockEl = time === undefined
    ? null
    : <span className={clock === 'start' ? ma.timeStart : ma.timeEnd}>{formatClock(time, day)}</span>
  return (
    <div className={clsx(ma.actions, className)} data-clock={clock}>
      {clock === 'start' && clockEl}
      {text !== '' && <ActionButton label={label} icon={copied ? <IconCheckOutlineRegular /> : <IconCopyOutlineRegular />} run={onCopy} />}
      {actions.map(action => <ActionButton key={action.label} {...action} />)}
      {clock === 'end' && <span className={ma.endInfo}>{clockEl}</span>}
    </div>
  )
}

function MessageEditor({ text, onCancel, onSave }: { text: string; onCancel: () => void; onSave: (text: string) => void }) {
  const [draft, setDraft] = useState(text)
  const empty = draft.trim() === ''
  return (
    <div className={local.editor}>
      <textarea
        className={clsx(ie.editor, local.editorInput)}
        autoFocus
        rows={3}
        aria-label={t.edit}
        value={draft}
        onChange={(event) => {
          setDraft(event.target.value)
        }}
        onKeyDown={(event) => {
          if (event.key === 'Escape') onCancel()
          else if (event.key === 'Enter' && event.ctrlKey && !event.nativeEvent.isComposing && !empty) {
            event.preventDefault()
            onSave(draft)
          }
        }}
      />
      <div className={local.editorButtons}>
        <Button variant="outline" onClick={onCancel}>{t.cancel}</Button>
        <Button variant="primary" disabled={empty} onClick={() => {
          onSave(draft)
        }}>{t.send}</Button>
      </div>
    </div>
  )
}

function UserMessage({ text, time, onEdit, onFork }: {
  text: string
  time: number | undefined
  onEdit: ((text: string) => void) | undefined
  onFork: () => void
}) {
  const [editing, setEditing] = useState(false)
  if (editing && onEdit !== undefined) {
    return (
      <div className={mi.userRow}>
        <MessageEditor
          text={text}
          onCancel={() => {
            setEditing(false)
          }}
          onSave={(next) => {
            setEditing(false)
            onEdit(next)
          }}
        />
      </div>
    )
  }
  const actions: Action[] = [
    ...(onEdit === undefined ? [] : [{ label: t.edit, icon: <IconEditOutlineRegular />, run: () => {
      setEditing(true)
    } }]),
    { label: t.branch, icon: <IconBranchOutlineRegular />, run: onFork },
  ]
  return (
    <div className={mi.userRow}>
      <div className={mi.userStack}>
        <div className={mi.bubble}>{text}</div>
      </div>
      <MessageActions text={text} time={time} clock="start" actions={actions} />
    </div>
  )
}

function AssistantMessage({ text, streaming }: { text: string; streaming: boolean }) {
  return (
    <div className={am.root}>
      <div className={am.body}>
        <MarkdownText text={text} streaming={streaming} labels={LABELS} />
      </div>
    </div>
  )
}

function TurnTail({ text, time, actions }: { text: string; time: number | undefined; actions: Action[] }) {
  return (
    <div className={tt.root} data-actions-reveal="always">
      <MessageActions text={text} time={time} clock="end" actions={actions} className={tt.actions} />
    </div>
  )
}

type FlowKind = 'user' | 'assistant' | 'reasoning' | 'tool' | 'notice' | 'turn-tail'

function FlowItem({ kind, children }: { kind: FlowKind; children: ReactNode }) {
  return (
    <div className={cv.flowItem} data-chat-flow-kind={kind} data-chat-group-part={kind === 'assistant' ? 'response' : undefined}>
      {children}
    </div>
  )
}

/** One reply's rows; a fragment, since flow items must stay direct children of the column. */
const Reply = memo(function Reply({ message, live, onRetry, onFork }: {
  message: ChatMessage
  live: boolean
  onRetry: (() => void) | undefined
  onFork: () => void
}) {
  const { parts, stopReason, error } = useMemo(() => replyParts(message), [message])
  const text = parts.flatMap(part => part.type === 'text' ? [part.text] : []).join('\n\n')
  const actions: Action[] = [
    ...(onRetry === undefined ? [] : [{ label: t.retry, icon: <IconRefreshOutlineRegular />, run: onRetry }]),
    { label: t.branch, icon: <IconBranchOutlineRegular />, run: onFork },
  ]
  return (
    <>
      {parts.map((part, at) => {
        if (part.type === 'reasoning') {
          return (
            <FlowItem key={at} kind="reasoning">
              <ReasoningRow part={part} running={live && part.endedAt === undefined} labels={LABELS} />
            </FlowItem>
          )
        }
        if (part.type === 'tool') {
          return <FlowItem key={at} kind="tool"><div className={cv.callRow}><ToolRow part={part} turnRunning={live} /></div></FlowItem>
        }
        return <FlowItem key={at} kind="assistant"><AssistantMessage text={part.text} streaming={live && at === parts.length - 1} /></FlowItem>
      })}
      {!live && (stopReason !== undefined || error !== undefined) && (
        <FlowItem kind="notice"><ReplyNotice stopReason={stopReason} error={error} /></FlowItem>
      )}
      {!live && <FlowItem kind="turn-tail"><TurnTail text={text} time={message.time} actions={actions} /></FlowItem>}
    </>
  )
})

function useFollowTail(scroller: HTMLElement | null, content: HTMLElement | null): [boolean, (behavior?: ScrollBehavior) => void] {
  const [following, setFollowing] = useState(true)
  const followingRef = useRef(true)

  useEffect(() => {
    if (scroller === null) return
    scroller.scrollTo({ top: scroller.scrollHeight })
    const onScroll = () => {
      const next = scroller.scrollHeight - scroller.scrollTop - scroller.clientHeight <= FOLLOW_SLACK
      followingRef.current = next
      setFollowing(next)
    }
    scroller.addEventListener('scroll', onScroll, { passive: true })
    return () => {
      scroller.removeEventListener('scroll', onScroll)
    }
  }, [scroller])

  useEffect(() => {
    if (scroller === null || content === null) return
    const observer = new ResizeObserver(() => {
      if (followingRef.current) scroller.scrollTo({ top: scroller.scrollHeight })
    })
    observer.observe(content)
    return () => {
      observer.disconnect()
    }
  }, [scroller, content])

  const returnToBottom = useCallback((behavior: ScrollBehavior = 'smooth') => {
    scroller?.scrollTo({ top: scroller.scrollHeight, behavior })
  }, [scroller])

  return [following, returnToBottom]
}

export interface ChatViewProps {
  messages: ChatMessage[]
  running: boolean
  activityAt: number
  /** Time spent waiting on a confirmation is not the model going quiet. */
  waiting: boolean
  scroller: HTMLElement | null
  /** Undefined while any turn runs. */
  onRetry: ((index: number) => void) | undefined
  onEdit: ((index: number, text: string) => void) | undefined
  onFork: (index: number) => void
}

export function ChatView({ messages, running, activityAt, waiting, scroller, onRetry, onEdit, onFork }: ChatViewProps) {
  const [column, setColumn] = useState<HTMLDivElement | null>(null)
  const [following, returnToBottom] = useFollowTail(scroller, column)

  // Sending brings the reader back to the end, wherever they had scrolled.
  const sent = messages.filter(message => message.role === 'user').length
  useLayoutEffect(() => {
    returnToBottom('instant')
  }, [sent, returnToBottom])

  return (
    <div className={cv.frame}>
      <div className={cv.root} data-chat-following-tail={following ? '' : undefined}>
        <div className={cv.scroll}>
          <div ref={setColumn} className={cv.column} data-chat-flow="">
            {messages.map((message, index) => {
              if (message.role === 'user') {
                return (
                  <FlowItem key={index} kind="user">
                    <UserMessage
                      text={message.content}
                      time={message.time}
                      onEdit={onEdit && ((text: string) => {
                        onEdit(index, text)
                      })}
                      onFork={() => {
                        onFork(index)
                      }}
                    />
                  </FlowItem>
                )
              }
              if (message.role !== 'assistant') return null
              return (
                <Reply
                  key={index}
                  message={message}
                  live={running && index === messages.length - 1}
                  onRetry={onRetry && messages[index - 1]?.role === 'user' ? () => {
                    onRetry(index)
                  } : undefined}
                  onFork={() => {
                    onFork(index)
                  }}
                />
              )
            })}
            {running && !waiting && <RunningStatus startedAt={messages.findLast(message => message.role === 'user')?.time} activityAt={activityAt} />}
          </div>
          <div className={cv.turnSpacer} aria-hidden="true" />
        </div>
      </div>
      {!following && (
        <div className={cv.toBottomSlot}>
          <button type="button" className={cv.toBottom} aria-label={t.toBottom} onClick={() => {
            returnToBottom()
          }}>
            <IconChevronDownOutlineRegular />
          </button>
        </div>
      )}
    </div>
  )
}
