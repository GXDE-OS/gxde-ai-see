/*
 * Heavily referenced DeepSeek Harness ui-chat ChatView, UserContent,
 * AssistantMarkdown, MessageIconActions and TurnTailNodeView
 * (MIT, see src/vendor/dsh/README.md).
 */
import { useCallback, useEffect, useLayoutEffect, useRef, useState } from 'react'
import type { ReactNode } from 'react'
import clsx from 'clsx'
import { formatClock, useCalendarDay } from '../chat/clock'
import { t } from '../chat/strings'
import type { ChatMessage } from '../ipc/bridge'
import { writeClipboard } from '../vendor/dsh/primitives/clipboard.ts'
import { IconCheckOutlineRegular, IconChevronDownOutlineRegular, IconCopyOutlineRegular } from '../vendor/dsh/primitives/icons/index.tsx'
import { MarkdownText } from '../vendor/dsh/primitives/markdown/MarkdownText.tsx'
import type { MarkdownLabels } from '../vendor/dsh/primitives/markdown/render.tsx'
import { Tooltip } from '../vendor/dsh/primitives/Tooltip.tsx'
import cv from '../vendor/dsh/chat/ChatView.module.css'
import mi from '../vendor/dsh/chat/MessageItem.module.css'
import am from '../vendor/dsh/chat/AssistantMarkdown.module.css'
import ma from '../vendor/dsh/chat/MessageIconActions.module.css'
import tt from '../vendor/dsh/chat/TurnTailNodeView.module.css'

const LABELS: MarkdownLabels = {
  code: {
    copyLabel: t.copy,
    copiedLabel: t.copied,
    toolbarLabels: { codeLabel: t.codeBlock, wrapLabel: t.wrap, unwrapLabel: t.unwrap },
  },
  footnotes: t.footnotes,
}

const FOLLOW_SLACK = 32

interface MessageActionsProps {
  text: string
  time: number | undefined
  clock: 'start' | 'end'
  className?: string
}

function MessageActions({ text, time, clock, className }: MessageActionsProps) {
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
      <Tooltip label={label} side="bottom">
        <button type="button" className={ma.action} aria-label={label} onClick={onCopy}>
          {copied ? <IconCheckOutlineRegular /> : <IconCopyOutlineRegular />}
        </button>
      </Tooltip>
      {clock === 'end' && <span className={ma.endInfo}>{clockEl}</span>}
    </div>
  )
}

function UserMessage({ text, time }: { text: string; time: number | undefined }) {
  return (
    <div className={mi.userRow}>
      <div className={mi.userStack}>
        <div className={mi.bubble}>{text}</div>
      </div>
      <MessageActions text={text} time={time} clock="start" />
    </div>
  )
}

function AssistantMessage({ text }: { text: string }) {
  return (
    <div className={am.root}>
      <div className={am.body}>
        <MarkdownText text={text} labels={LABELS} />
      </div>
    </div>
  )
}

function TurnTail({ text, time }: { text: string; time: number | undefined }) {
  return (
    <div className={tt.root} data-actions-reveal="always">
      <MessageActions text={text} time={time} clock="end" className={tt.actions} />
    </div>
  )
}

function FlowItem({ kind, children }: { kind: 'user' | 'assistant' | 'turn-tail'; children: ReactNode }) {
  return (
    <div className={cv.flowItem} data-chat-flow-kind={kind} data-chat-group-part={kind === 'assistant' ? 'response' : undefined}>
      {children}
    </div>
  )
}

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
  scroller: HTMLElement | null
}

export function ChatView({ messages, running, scroller }: ChatViewProps) {
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
            {messages.flatMap((message, index) => {
              if (message.role === 'user') {
                return [<FlowItem key={index} kind="user"><UserMessage text={message.content} time={message.time} /></FlowItem>]
              }
              if (message.role !== 'assistant' || message.content === '') return []
              const reply = <FlowItem key={index} kind="assistant"><AssistantMessage text={message.content} /></FlowItem>
              if (running && index === messages.length - 1) return [reply]
              return [
                reply,
                <FlowItem key={`${index}:tail`} kind="turn-tail"><TurnTail text={message.content} time={message.time} /></FlowItem>,
              ]
            })}
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
