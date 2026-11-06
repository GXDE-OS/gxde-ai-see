/*
 * Markup follows DeepSeek Harness ui-conversation InputBar and HeroShell
 * (MIT, see src/vendor/dsh/README.md). The Lexical editor is a textarea here.
 */
import { useRef, useState } from 'react'
import type { KeyboardEvent, MouseEvent } from 'react'
import clsx from 'clsx'
import appIcon from '../assets/app-icon.png'
import { t } from '../chat/strings'
import ib from '../vendor/dsh/conversation/InputBar.module.css'
import hs from '../vendor/dsh/conversation/HeroShell.module.css'
import local from './Composer.module.css'

const SEND_GLYPH = (
  <svg viewBox="0 0 16 16" width="16" height="16" aria-hidden>
    <path d="M8.3125 0.980183C8.66767 1.0531 8.97902 1.20418 9.2627 1.43233C9.48724 1.61297 9.73029 1.85793 9.97949 2.10714L14.707 6.83468L13.293 8.24874L9 3.95577V15.0417H7V3.95577L2.70703 8.24874L1.29297 6.83468L6.02051 2.10714C6.26971 1.85793 6.51277 1.61297 6.7373 1.43233C6.97662 1.23986 7.28445 1.04402 7.6875 0.980183C7.8973 0.947006 8.1031 0.95516 8.3125 0.980183Z" fill="currentColor" />
  </svg>
)

const keepFocus = (event: MouseEvent) => {
  event.preventDefault()
}

export interface ComposerProps {
  variant: 'hero' | 'composer'
  onSend?: (text: string) => void
}

export function Composer({ variant, onSend }: ComposerProps) {
  const [text, setText] = useState('')
  const textarea = useRef<HTMLTextAreaElement>(null)

  const empty = text.trim() === ''
  const submit = () => {
    if (empty || onSend === undefined) return
    onSend(text)
    setText('')
  }
  const onKeyDown = (event: KeyboardEvent<HTMLTextAreaElement>) => {
    if (event.key !== 'Enter' || event.shiftKey || event.nativeEvent.isComposing) return
    event.preventDefault()
    submit()
  }
  const placeholder = variant === 'hero' ? t.placeholderHero : t.placeholder

  return (
    <div className={clsx(ib.root, variant === 'hero' && ib.hero)}>
      <div
        className={ib.card}
        data-composer-card
        onMouseDown={(event) => {
          if (event.target !== textarea.current) {
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
                setText(event.target.value)
              }}
              onKeyDown={onKeyDown}
            />
            {text === '' && <div aria-hidden className={ib.placeholder}>{placeholder}</div>}
          </div>
        </div>
        <div className={ib.row}>
          <div className={ib.tools} />
          <div className={ib.trailing}>
            <button
              type="button"
              className={ib.primary}
              aria-label={t.send}
              disabled={empty || onSend === undefined}
              onMouseDown={keepFocus}
              onClick={submit}
            >
              {SEND_GLYPH}
            </button>
          </div>
        </div>
      </div>
      <div className={ib.dock} data-composer-dock />
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
