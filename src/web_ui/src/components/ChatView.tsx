/*
 * Heavily referenced DeepSeek Harness ui-chat ChatView, UserContent and
 * AssistantMarkdown (MIT, see src/vendor/dsh/README.md).
 */
import type { ReactNode } from 'react'
import { t } from '../chat/strings'
import type { ChatMessage } from '../ipc/bridge'
import { MarkdownText } from '../vendor/dsh/primitives/markdown/MarkdownText.tsx'
import type { MarkdownLabels } from '../vendor/dsh/primitives/markdown/render.tsx'
import cv from '../vendor/dsh/chat/ChatView.module.css'
import mi from '../vendor/dsh/chat/MessageItem.module.css'
import am from '../vendor/dsh/chat/AssistantMarkdown.module.css'

const LABELS: MarkdownLabels = {
  code: {
    copyLabel: t.copy,
    copiedLabel: t.copied,
    toolbarLabels: { codeLabel: t.codeBlock, wrapLabel: t.wrap, unwrapLabel: t.unwrap },
  },
  footnotes: t.footnotes,
}

function UserMessage({ text }: { text: string }) {
  return (
    <div className={mi.userRow}>
      <div className={mi.userStack}>
        <div className={mi.bubble}>{text}</div>
      </div>
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

function FlowItem({ kind, children }: { kind: 'user' | 'assistant'; children: ReactNode }) {
  return (
    <div className={cv.flowItem} data-chat-flow-kind={kind} data-chat-group-part={kind === 'assistant' ? 'response' : undefined}>
      {children}
    </div>
  )
}

export function ChatView({ messages }: { messages: ChatMessage[] }) {
  return (
    <div className={cv.frame}>
      <div className={cv.root}>
        <div className={cv.scroll}>
          <div className={cv.column} data-chat-flow="">
            {messages.map((message, index) => {
              if (message.role === 'user') {
                return <FlowItem key={index} kind="user"><UserMessage text={message.content} /></FlowItem>
              }
              if (message.role === 'assistant' && message.content !== '') {
                return <FlowItem key={index} kind="assistant"><AssistantMessage text={message.content} /></FlowItem>
              }
              return null
            })}
          </div>
          <div className={cv.turnSpacer} aria-hidden="true" />
        </div>
      </div>
    </div>
  )
}
