/*
 * Heavily referenced DeepSeek Harness ui-conversation ConversationMainPanel,
 * ConversationHeader and ConversationContent (MIT, see src/vendor/dsh/README.md).
 */
import { useEffect, useState } from 'react'
import clsx from 'clsx'
import { t } from '../chat/strings'
import type { Conversation } from '../ipc/bridge'
import cr from '../vendor/dsh/conversation/ConversationRoot.module.css'
import { ChatView } from './ChatView'
import { Composer, HeroShell } from './Composer'

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

export interface ConversationPanelProps {
  conversation: Conversation | undefined
}

export function ConversationPanel({ conversation }: ConversationPanelProps) {
  const hero = conversation === undefined
  const phase = hero ? 'hero' : 'active'
  const [body, setBody] = useState<HTMLDivElement | null>(null)
  const [scroller, setScroller] = useState<HTMLDivElement | null>(null)
  const [seat, setSeat] = useState<HTMLDivElement | null>(null)
  useColumnWidth(body)
  useComposerHeight(scroller, seat)

  return (
    <div className={cr.root} data-phase={phase}>
      <header className={clsx(cr.header, hero && cr.headerBlank, hero && cr.headerSessionless)}>
        <div className={cr.headerLeading} />
        <div className={cr.titleRow}>
          {!hero && (
            <div className={cr.titleCluster}>
              <nav className={cr.crumbs}>
                <span className={cr.crumbSeg}>
                  <span className={clsx(cr.crumb, cr.crumbCurrent)}>{conversation.title || t.untitled}</span>
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
              <ChatView key={conversation.id} messages={conversation.messages} scroller={scroller} />
            </div>
          )}
          <div ref={setSeat} className={cr.composerSeat} data-composer-seat="">
            <div className={clsx(cr.composerStack, hero && cr.composerHero)}>
              {hero && <HeroShell />}
              <Composer variant={hero ? 'hero' : 'composer'} />
            </div>
          </div>
        </div>
      </div>
    </div>
  )
}
