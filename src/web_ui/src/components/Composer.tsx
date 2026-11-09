/*
 * Markup follows DeepSeek Harness ui-conversation InputBar and HeroShell
 * (MIT, see src/vendor/dsh/README.md). The Lexical editor is a textarea here.
 */
import { useEffect, useRef } from 'react'
import type { KeyboardEvent, MouseEvent, ReactNode } from 'react'
import clsx from 'clsx'
import appIcon from '../assets/app-icon.png'
import sendIcon from '../assets/icons/send.svg'
import { t } from '../chat/strings'
import ib from '../vendor/dsh/conversation/InputBar.module.css'
import hs from '../vendor/dsh/conversation/HeroShell.module.css'
import local from './Composer.module.css'
import { MaskIcon } from './MaskIcon'

const STOP_GLYPH = (
  <svg viewBox="0 0 16 16" width="16" height="16" aria-hidden>
    <rect x="3" y="3" width="10" height="10" rx="3" fill="currentColor" />
  </svg>
)

const keepFocus = (event: MouseEvent) => {
  event.preventDefault()
}

export interface ComposerProps {
  variant: 'hero' | 'composer'
  text: string
  onTextChange: (text: string) => void
  running?: boolean
  onSend?: ((text: string) => void) | undefined
  onStop?: () => void
  /** Pickers on the left of the action row. */
  tools?: ReactNode
  /** One line under the card. */
  notice?: string | undefined
  /** Bumped to focus the input with the caret at the end. */
  focusRequest?: number
}

export function Composer({ variant, text, onTextChange, running = false, onSend, onStop, tools, notice, focusRequest = 0 }: ComposerProps) {
  const textarea = useRef<HTMLTextAreaElement>(null)
  useEffect(() => {
    const input = textarea.current
    if (focusRequest === 0 || input === null) return
    input.focus()
    input.setSelectionRange(input.value.length, input.value.length)
  }, [focusRequest])

  const empty = text.trim() === ''
  const submit = () => {
    if (running || empty || onSend === undefined) return
    onSend(text)
    onTextChange('')
  }
  const onKeyDown = (event: KeyboardEvent<HTMLTextAreaElement>) => {
    if (event.key !== 'Enter' || !event.ctrlKey || event.nativeEvent.isComposing) return
    event.preventDefault()
    submit()
  }
  const placeholder = variant === 'hero' ? t.placeholderHero : t.placeholder

  return (
    <div className={clsx(ib.root, variant === 'hero' && ib.hero)}>
      <div
        className={clsx(ib.card, local.card)}
        data-composer-card
        onMouseDown={(event) => {
          if (event.target !== textarea.current && (event.target as Element).closest('button') === null) {
            event.preventDefault()
            textarea.current?.focus()
          }
        }}
      >
        <div className={ib.scroll}>
          <div className={ib.grow}>
            <textarea
              ref={textarea}
              className={clsx(ib.input, local.textarea)}
              rows={1}
              autoFocus
              value={text}
              aria-label={placeholder}
              onChange={(event) => {
                onTextChange(event.target.value)
              }}
              onKeyDown={onKeyDown}
            />
            {text === '' && <div aria-hidden className={ib.placeholder}>{placeholder}</div>}
          </div>
        </div>
        <div className={ib.row}>
          <div className={ib.tools}>{tools}</div>
          <div className={ib.trailing}>
            {running
              ? (
                  <button type="button" className={clsx(ib.primary, local.send)} aria-label={t.stop} onMouseDown={keepFocus} onClick={onStop}>
                    {STOP_GLYPH}
                  </button>
                )
              : (
                  <button
                    type="button"
                    className={clsx(ib.primary, local.send)}
                    aria-label={t.send}
                    disabled={empty || onSend === undefined}
                    onMouseDown={keepFocus}
                    onClick={submit}
                  >
                    <MaskIcon src={sendIcon} size={18} />
                  </button>
                )}
          </div>
        </div>
      </div>
      <div className={ib.dock} data-composer-dock>
        {notice !== undefined && <span className={local.notice}>{notice}</span>}
      </div>
    </div>
  )
}

export function HeroShell() {
  return (
    <div className={hs.root}>
      <div className={hs.stack}>
        <div className={hs.headline}>
          <span className={hs.fishHitbox}>
            <img className={clsx(hs.fish, local.heroIcon)} src={appIcon} alt="" />
          </span>
          <span className={hs.titleGroup}>
            <span>{t.heroHeadline}</span>
          </span>
        </div>
        <div className={hs.body} />
      </div>
    </div>
  )
}
