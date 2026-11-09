/*
 * Reasoning after LibreChat's Thinking and Reasoning parts
 * (MIT, Copyright (c) 2026 LibreChat).
 */
import { useCallback, useEffect, useId, useLayoutEffect, useMemo, useRef, useState } from 'react'
import clsx from 'clsx'
import chevronIcon from '../assets/icons/chevron_down.svg'
import copyIcon from '../assets/icons/copy.svg'
import thinkIcon from '../assets/icons/think_content.svg'
import { t } from '../chat/strings'
import { writeClipboard } from '../vendor/dsh/primitives/clipboard.ts'
import { IconCheckOutlineRegular } from '../vendor/dsh/primitives/icons/index.tsx'
import { TextShimmer } from '../vendor/dsh/primitives/TextShimmer.tsx'
import { MaskIcon } from './MaskIcon'
import css from './Thinking.module.css'

const PEEK_SENTENCES = 4

function lastSentences(text: string): string {
  const tail = text.slice(-1200)
  return tail.split(/(?<=[.!?。！？])\s*/).slice(-PEEK_SENTENCES).join('').trim()
}

/** The newest few sentences of a thought still streaming, scrolled to the end. */
function Peek({ text }: { text: string }) {
  const ref = useRef<HTMLDivElement>(null)
  const peek = useMemo(() => lastSentences(text), [text])
  useLayoutEffect(() => {
    if (ref.current) ref.current.scrollTop = ref.current.scrollHeight
  }, [peek])
  if (peek === '') return null
  return (
    <div className={css.peek} aria-hidden="true">
      <div ref={ref} className={css.peekText}>{peek}</div>
    </div>
  )
}

function CopyThoughts({ text }: { text: string }) {
  const [copied, setCopied] = useState(false)
  const timer = useRef<number | undefined>(undefined)
  useEffect(() => () => {
    window.clearTimeout(timer.current)
  }, [])
  const copy = useCallback(() => {
    void writeClipboard(text).then((ok) => {
      if (!ok) return
      setCopied(true)
      window.clearTimeout(timer.current)
      timer.current = window.setTimeout(() => {
        setCopied(false)
      }, 2000)
    })
  }, [text])
  return (
    <button type="button" className={css.copy} aria-label={copied ? t.copied : t.copyThoughts} onClick={copy}>
      {copied ? <IconCheckOutlineRegular size={16} /> : <MaskIcon src={copyIcon} size={16} />}
    </button>
  )
}

export function Thinking({ text, running }: { text: string; running: boolean }) {
  const [expanded, setExpanded] = useState(false)
  const bodyId = useId()
  const label = running ? t.thinking : t.thoughts
  if (text.trim() === '') return null
  return (
    <div className={css.root}>
      <div className={css.header}>
        <button
          type="button"
          className={css.toggle}
          aria-expanded={expanded}
          aria-controls={bodyId}
          onClick={() => {
            setExpanded(open => !open)
          }}
        >
          <span className={css.glyph}>
            <span className={css.bulb}><MaskIcon src={thinkIcon} size={16} /></span>
            <span className={clsx(css.chevron, expanded && css.open)}><MaskIcon src={chevronIcon} size={16} /></span>
          </span>
          <span className={css.label}>{running ? <TextShimmer>{label}</TextShimmer> : label}</span>
        </button>
        {expanded && <CopyThoughts text={text} />}
      </div>
      {!expanded && running && <Peek text={text} />}
      <div
        id={bodyId}
        className={clsx(css.fold, expanded && css.open)}
        role="group"
        aria-label={label}
        aria-hidden={!expanded || undefined}
        inert={!expanded}
      >
        <div className={css.foldInner}>
          <div className={css.content}>{text}</div>
        </div>
      </div>
    </div>
  )
}
