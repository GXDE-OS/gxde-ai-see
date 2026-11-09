/*
 * Message Markdown after LibreChat's Markdown, MarkdownComponents and
 * CodeBlock (MIT, Copyright (c) 2026 LibreChat).
 */
import { isValidElement, memo, useCallback, useDeferredValue, useEffect, useRef, useState } from 'react'
import type { ReactElement, ReactNode } from 'react'
import clsx from 'clsx'
import ReactMarkdown from 'react-markdown'
import type { Components, Options } from 'react-markdown'
import rehypeHighlight from 'rehype-highlight'
import rehypeKatex from 'rehype-katex'
import remarkGfm from 'remark-gfm'
import remarkMath from 'remark-math'
import 'katex/dist/katex.min.css'
import copyIcon from '../assets/icons/copy.svg'
import unwrapIcon from '../assets/icons/unwrap.svg'
import wrapIcon from '../assets/icons/wrap.svg'
import { t } from '../chat/strings'
import { writeClipboard } from '../vendor/dsh/primitives/clipboard.ts'
import { IconCheckOutlineRegular } from '../vendor/dsh/primitives/icons/index.tsx'
import { MaskIcon } from './MaskIcon'
import css from './Markdown.module.css'

const REMARK: Options['remarkPlugins'] = [remarkGfm, remarkMath]
const REHYPE: Options['rehypePlugins'] = [rehypeKatex, [rehypeHighlight, { detect: false }]]

function CodeBlock({ lang, children }: { lang: string; children: ReactNode }) {
  const code = useRef<HTMLElement>(null)
  const [copied, setCopied] = useState(false)
  const [wrapped, setWrapped] = useState(false)
  const timer = useRef<number | undefined>(undefined)
  useEffect(() => () => {
    window.clearTimeout(timer.current)
  }, [])
  const copy = useCallback(() => {
    void writeClipboard(code.current?.textContent ?? '').then((ok) => {
      if (!ok) return
      setCopied(true)
      window.clearTimeout(timer.current)
      timer.current = window.setTimeout(() => {
        setCopied(false)
      }, 1000)
    })
  }, [])
  return (
    <div className={css.codeBlock}>
      <div className={css.codeBar}>
        <span className={css.codeLang}>{lang}</span>
        <span className={css.codeTools}>
          <button
            type="button"
            className={css.codeButton}
            aria-label={wrapped ? t.unwrap : t.wrap}
            aria-pressed={wrapped}
            onClick={() => {
              setWrapped(on => !on)
            }}
          >
            <MaskIcon src={wrapped ? unwrapIcon : wrapIcon} size={14} />
          </button>
          <button type="button" className={css.codeButton} aria-label={copied ? t.copied : t.copy} onClick={copy}>
            {copied ? <IconCheckOutlineRegular size={14} /> : <MaskIcon src={copyIcon} size={14} />}
          </button>
        </span>
      </div>
      <div className={css.codeBody} data-wrapped={wrapped || undefined} dir="ltr">
        <code ref={code} className={clsx('hljs', `language-${lang}`)}>{children}</code>
      </div>
    </div>
  )
}

type CodeElement = ReactElement<{ className?: string; children?: ReactNode }>

const COMPONENTS: Components = {
  pre({ children }) {
    if (!isValidElement(children)) return <pre>{children}</pre>
    const { className, children: code } = (children as CodeElement).props
    const lang = /language-([\w+#-]+)/.exec(className ?? '')?.[1] ?? 'text'
    return <CodeBlock lang={lang}>{code}</CodeBlock>
  },
  a({ href, children }) {
    return <a href={href} target="_blank" rel="noreferrer">{children}</a>
  },
  p({ children }) {
    return <p dir="auto" className={css.paragraph}>{children}</p>
  },
  table({ children }) {
    return (
      <div className={css.tableWrapper}>
        <table>{children}</table>
      </div>
    )
  },
}

export const Markdown = memo(function Markdown({ text, compact = false }: { text: string; compact?: boolean }) {
  // Streaming re-parses the whole reply, so let typing and scrolling go first.
  const source = useDeferredValue(text)
  return (
    <div className={clsx(css.root, compact && css.compact)}>
      <ReactMarkdown remarkPlugins={REMARK} rehypePlugins={REHYPE} components={COMPONENTS}>
        {source}
      </ReactMarkdown>
    </div>
  )
})
