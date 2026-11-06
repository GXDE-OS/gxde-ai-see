import type { ChatMessage, ReplyPart, StopReason, ToolCallStatus } from '../ipc/bridge'
import { t } from './strings'

function settleReasoning(parts: ReplyPart[], time: number): ReplyPart[] {
  const last = parts.at(-1)
  if (last?.type !== 'reasoning' || last.endedAt !== undefined) return parts
  return [...parts.slice(0, -1), { ...last, endedAt: time }]
}

export function appendText(reply: ChatMessage, text: string, time: number): ChatMessage {
  const parts = settleReasoning(reply.parts ?? [], time)
  const last = parts.at(-1)
  return {
    ...reply,
    content: reply.content + text,
    parts: last?.type === 'text'
      ? [...parts.slice(0, -1), { ...last, text: last.text + text }]
      : [...parts, { type: 'text', text }],
  }
}

export function appendReasoning(reply: ChatMessage, text: string, time: number): ChatMessage {
  const parts = reply.parts ?? []
  const last = parts.at(-1)
  return {
    ...reply,
    parts: last?.type === 'reasoning' && last.endedAt === undefined
      ? [...parts.slice(0, -1), { ...last, text: last.text + text }]
      : [...parts, { type: 'reasoning', text, startedAt: time }],
  }
}

export function updateTool(
  reply: ChatMessage, id: string, title: string, kind: string, status: ToolCallStatus, time: number,
): ChatMessage {
  const parts = settleReasoning(reply.parts ?? [], time)
  const index = parts.findIndex(part => part.type === 'tool' && part.id === id)
  // A tool_call_update carries only the fields that changed.
  if (index === -1) return { ...reply, parts: [...parts, { type: 'tool', id, title, kind, status: status || 'pending' }] }
  const tool = parts[index] as Extract<ReplyPart, { type: 'tool' }>
  const next = { ...tool, title: title || tool.title, kind: kind || tool.kind, status: status || tool.status }
  return { ...reply, parts: parts.map((part, at) => at === index ? next : part) }
}

export function closeReply(reply: ChatMessage, time: number, stopReason?: StopReason, error?: string): ChatMessage {
  return {
    ...reply,
    time,
    parts: settleReasoning(reply.parts ?? [], time),
    ...(stopReason === undefined || stopReason === 'end_turn' ? {} : { stopReason }),
    ...(error === undefined ? {} : { error }),
  }
}

const LEGACY_NOTES: [string, StopReason][] = [
  [t.stopCancelled, 'cancelled'],
  [t.stopMaxTokens, 'max_tokens'],
  [t.stopMaxTurns, 'max_turn_requests'],
  [t.stopRefusal, 'refusal'],
]

/**
 * Replies saved by the native window, or before parts existed, carry tool
 * lines, stop notes and ⚠️ errors inside their content.
 */
export function replyParts(reply: ChatMessage): Pick<ChatMessage, 'stopReason' | 'error'> & { parts: ReplyPart[] } {
  if (reply.parts !== undefined) return { parts: reply.parts, stopReason: reply.stopReason, error: reply.error }
  let content = reply.content.trimEnd()
  let stopReason: StopReason | undefined
  let error: string | undefined
  for (const [note, reason] of LEGACY_NOTES) {
    if (!content.endsWith(note)) continue
    content = content.slice(0, -note.length).trimEnd()
    stopReason = reason
    break
  }
  const separator = content.lastIndexOf('\n\n⚠️ ')
  const errorAt = content.startsWith('⚠️ ') ? 0 : separator === -1 ? -1 : separator + 2
  if (errorAt !== -1 && !content.slice(errorAt).startsWith('⚠️ 工具未完成')) {
    error = content.slice(errorAt + '⚠️ '.length)
    content = content.slice(0, errorAt).trimEnd()
  }

  const parts: ReplyPart[] = []
  for (const block of content.split(/\n{2,}/)) {
    const tool = /^🔧 (.+)$/.exec(block)
    const failed = /^⚠️ 工具未完成：(.+)$/.exec(block)
    const last = parts.at(-1)
    if (tool !== null) {
      parts.push({ type: 'tool', id: String(parts.length), title: tool[1]!, kind: '', status: 'completed' })
    } else if (failed !== null) {
      const match = parts.findLast(part => part.type === 'tool' && part.title === failed[1])
      if (match?.type === 'tool') match.status = 'failed'
    } else if (last?.type === 'text') {
      last.text += `\n\n${block}`
    } else if (block !== '') {
      parts.push({ type: 'text', text: block })
    }
  }
  return { parts, stopReason, error }
}
