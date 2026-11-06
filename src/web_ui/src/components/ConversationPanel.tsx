/*
 * Markup follows DeepSeek Harness ui-conversation ConversationMainPanel,
 * ConversationHeader and ConversationContent (MIT, see src/vendor/dsh/README.md).
 */
import { useEffect, useState } from 'react'
import clsx from 'clsx'
import { t } from '../chat/strings'
import cr from '../vendor/dsh/conversation/ConversationRoot.module.css'
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

export interface ConversationPanelProps {
  /** Title of the open conversation; undefined shows the hero. */
  title: string | undefined
}

export function ConversationPanel({ title }: ConversationPanelProps) {
  const hero = title === undefined
  const phase = hero ? 'hero' : 'active'
  const [body, setBody] = useState<HTMLDivElement | null>(null)
  useColumnWidth(body)

  return (
    <div className={cr.root} data-phase={phase}>
      <header className={clsx(cr.header, hero && cr.headerBlank, hero && cr.headerSessionless)}>
        <div className={cr.headerLeading} />
        <div className={cr.titleRow}>
          {!hero && (
            <div className={cr.titleCluster}>
              <nav className={cr.crumbs}>
                <span className={cr.crumbSeg}>
                  <span className={clsx(cr.crumb, cr.crumbCurrent)}>{title || t.untitled}</span>
                </span>
              </nav>
            </div>
          )}
        </div>
      </header>
      <div ref={setBody} className={cr.body} data-conversation-content="" data-content-phase={phase}>
        <div className={cr.scrollBody} data-conversation-scroll="">
          {!hero && <div className={cr.viewArea} />}
          <div className={cr.composerSeat} data-composer-seat="">
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
