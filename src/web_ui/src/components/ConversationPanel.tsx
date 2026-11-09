/*
 * Heavily referenced DeepSeek Harness ui-conversation ConversationMainPanel,
 * ConversationHeader and ConversationContent (MIT, see src/vendor/dsh/README.md).
 */
import { useEffect, useState } from 'react'
import type { ReactNode } from 'react'
import clsx from 'clsx'
import { t } from '../chat/strings'
import type { Approval } from '../chat/useChat'
import type { Conversation } from '../ipc/bridge'
import cr from '../vendor/dsh/conversation/ConversationRoot.module.css'
import { ApprovalCard } from './ApprovalCard'
import { ChatView } from './ChatView'
import { Composer, HeroShell } from './Composer'
import local from './ConversationPanel.module.css'

function useColumnWidth(body: HTMLElement | null) {
  useEffect(() => {
    if (body === null) return
    const observer = new ResizeObserver(([entry]) => {
      if (entry !== undefined) body.style.setProperty('--dsh-conversation-column-width', `${entry.contentRect.width}px`)
    })
    observer.observe(body)
    return () => {
      observer.disconnect()
    }
  }, [body])
}

function useComposerHeight(scroller: HTMLElement | null, seat: HTMLElement | null) {
  useEffect(() => {
    if (scroller === null || seat === null) return
    const observer = new ResizeObserver(() => {
      scroller.style.setProperty('--dsh-composer-height', `${seat.offsetHeight}px`)
    })

    observer.observe(seat)
    return () => {
      observer.disconnect()
    }
  }, [scroller, seat])
}

function useScrolled(scroller: HTMLElement | null): boolean {
  const [scrolled, setScrolled] = useState(false)
  useEffect(() => {
    if (scroller === null) return
    const update = () => {
      setScrolled(scroller.scrollTop > 0)
    }
    update()
    scroller.addEventListener('scroll', update, { passive: true })
    return () => {
      scroller.removeEventListener('scroll', update)
    }
  }, [scroller])
  return scrolled
}

export interface ConversationPanelProps {
  conversation: Conversation | undefined
  running: boolean
  /** Another conversation's turn is running. */
  busy: boolean
  activityAt: number
  /** The current conversation's pending confirmation. */
  approval: Approval | undefined
  draft: string
  onDraftChange: (text: string) => void
  tools: ReactNode
  notice: string | undefined
  focusRequest: number
  onSend: ((text: string) => void) | undefined
  onStop: () => void
  onAnswer: (key: string, optionId: string) => void
  onRetry: (index: number) => void
  onEdit: (index: number, text: string) => void
  onFork: (index: number) => void
}

export function ConversationPanel({
  conversation, running, busy, activityAt, approval, draft, onDraftChange, tools, notice, focusRequest, onSend, onStop, onAnswer, onRetry, onEdit, onFork,
}: ConversationPanelProps) {
  const hero = conversation === undefined
  const phase = hero ? 'hero' : 'active'
  const [body, setBody] = useState<HTMLDivElement | null>(null)
  const [scroller, setScroller] = useState<HTMLDivElement | null>(null)
  const [seat, setSeat] = useState<HTMLDivElement | null>(null)
  useColumnWidth(body)
  useComposerHeight(scroller, seat)
  const scrolled = useScrolled(scroller)

  return (
    <div className={cr.root} data-phase={phase}>
      <header
        className={clsx(cr.header, hero ? [cr.headerBlank, cr.headerSessionless] : local.header)}
        data-scrolled={scrolled ? '' : undefined}
      >
        <div className={cr.headerLeading} />
        <div className={cr.titleRow}>
          {!hero && (
            <div className={cr.titleCluster}>
              <nav className={cr.crumbs}>
                <span className={cr.crumbSeg}>
                  <span className={clsx(cr.crumb, cr.crumbCurrent, local.title)}>{conversation.title || t.untitled}</span>
                </span>
              </nav>
            </div>
          )}
        </div>
      </header>
      <div ref={setBody} className={cr.body} data-conversation-content="" data-content-phase={phase}>
        <div ref={setScroller} className={cr.scrollBody} data-conversation-scroll="">
          {!hero && (
            <div className={cr.viewArea}>
              <ChatView
                key={conversation.id}
                messages={conversation.messages}
                running={running}
                activityAt={activityAt}
                waiting={approval !== undefined}
                scroller={scroller}
                onRetry={busy || running ? undefined : onRetry}
                onEdit={busy || running ? undefined : onEdit}
                onFork={onFork}
              />
            </div>
          )}
          <div ref={setSeat} className={cr.composerSeat} data-composer-seat="">
            <div className={clsx(cr.composerStack, hero && cr.composerHero)}>
              {hero && <HeroShell />}
              {approval !== undefined
                ? (
                    <ApprovalCard
                      key={approval.key}
                      approval={approval}
                      onAnswer={(optionId) => {
                        onAnswer(approval.key, optionId)
                      }}
                    />
                  )
                : (
                    <Composer
                      variant={hero ? 'hero' : 'composer'}
                      text={draft}
                      onTextChange={onDraftChange}
                      running={running}
                      onSend={busy ? undefined : onSend}
                      onStop={onStop}
                      tools={tools}
                      notice={notice}
                      focusRequest={focusRequest}
                    />
                  )}
            </div>
          </div>
        </div>
      </div>
    </div>
  )
}
