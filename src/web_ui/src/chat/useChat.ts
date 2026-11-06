import { useCallback, useEffect, useReducer, useRef, useState } from 'react'
import type { Bridge, ChatMessage, Conversation, ConversationStore, StopReason } from '../ipc/bridge'
import { connectBridge } from '../ipc/bridge'
import { t } from './strings'

interface State {
  loaded: boolean
  conversations: Conversation[]
  currentId: string | null
  running: string | null
  /** Bumped by every change worth writing to chat.json. */
  revision: number
}

type Action =
  | { type: 'load'; store: ConversationStore }
  | { type: 'open'; id: string | null }
  | { type: 'rename'; id: string; title: string }
  | { type: 'remove'; ids: string[] }
  | { type: 'send'; conversation: Conversation }
  | { type: 'delta'; id: string; text: string }
  | { type: 'end'; id: string; note: string; time: number }

const STOP_NOTES: Partial<Record<StopReason, string>> = {
  max_tokens: t.stopMaxTokens,
  max_turn_requests: t.stopMaxTurns,
  refusal: t.stopRefusal,
  cancelled: t.stopCancelled,
}

const byRecent = (a: Conversation, b: Conversation) => b.updatedAt - a.updatedAt

function updateReply(state: State, id: string, update: (reply: ChatMessage) => ChatMessage): Conversation[] {
  return state.conversations.map((conversation) => {
    const reply = conversation.messages.at(-1)
    if (conversation.id !== id || reply?.role !== 'assistant') return conversation
    return { ...conversation, messages: [...conversation.messages.slice(0, -1), update(reply)] }
  })
}

function reduce(state: State, action: Action): State {
  switch (action.type) {
    case 'load':
      return { ...state, loaded: true, conversations: [...(action.store.conversations ?? [])].sort(byRecent) }
    case 'open':
      return { ...state, currentId: action.id }
    case 'rename':
      return {
        ...state,
        conversations: state.conversations.map(conversation =>
          conversation.id === action.id ? { ...conversation, title: action.title } : conversation),
        revision: state.revision + 1,
      }
    case 'remove':
      return {
        ...state,
        conversations: state.conversations.filter(conversation => !action.ids.includes(conversation.id)),
        currentId: state.currentId !== null && action.ids.includes(state.currentId) ? null : state.currentId,
        revision: state.revision + 1,
      }
    case 'send': {
      const others = state.conversations.filter(conversation => conversation.id !== action.conversation.id)
      return {
        ...state,
        conversations: [action.conversation, ...others],
        currentId: action.conversation.id,
        running: action.conversation.id,
        revision: state.revision + 1,
      }
    }
    case 'delta':
      if (state.running !== action.id) return state
      return { ...state, conversations: updateReply(state, action.id, reply => ({ ...reply, content: reply.content + action.text })) }
    case 'end': {
      // Same shape the native window wrote: notes follow the reply after a blank line.
      const conversations = updateReply(state, action.id, (reply) => {
        const content = action.note === '' || reply.content.trim() === ''
          ? reply.content + action.note
          : `${reply.content}\n\n${action.note}`
        return { ...reply, content, time: action.time }
      }).map(conversation => conversation.id === action.id ? { ...conversation, updatedAt: action.time } : conversation)
      return {
        ...state,
        conversations: conversations.sort(byRecent),
        running: state.running === action.id ? null : state.running,
        revision: state.revision + 1,
      }
    }
  }
}

const INITIAL: State = { loaded: false, conversations: [], currentId: null, running: null, revision: 0 }

export interface Chat {
  conversations: Conversation[]
  current: Conversation | undefined
  running: boolean
  open: (id: string | null) => void
  rename: (id: string, title: string) => void
  remove: (ids: string[]) => void
  send: ((text: string) => void) | undefined
  stop: () => void
}

export function useChat(): Chat {
  const [state, dispatch] = useReducer(reduce, INITIAL)
  const [bridge, setBridge] = useState<Bridge | null>(null)

  useEffect(() => {
    let alive = true
    const unsubscribe: (() => void)[] = []
    void connectBridge().then(async (connected) => {
      if (!alive || connected === null) return
      unsubscribe.push(
        connected.on('delta', (id, text) => {
          dispatch({ type: 'delta', id, text })
        }),
        connected.on('finished', (id, stopReason) => {
          dispatch({ type: 'end', id, note: STOP_NOTES[stopReason] ?? '', time: Date.now() })
        }),
        connected.on('failed', (id, error) => {
          // The ⚠️ prefix keeps the reply out of the context replayed to a new session.
          dispatch({ type: 'end', id, note: `⚠️ ${error}`, time: Date.now() })
        }),
      )
      setBridge(connected)
      const store = await connected.loadConversations()
      if (alive) dispatch({ type: 'load', store })
    })
    return () => {
      alive = false
      for (const off of unsubscribe) off()
    }
  }, [])

  const saved = useRef(0)
  useEffect(() => {
    if (bridge === null || !state.loaded || state.revision === saved.current) return
    saved.current = state.revision
    void bridge.saveConversations({ version: 1, conversations: state.conversations })
  }, [bridge, state])

  const current = state.conversations.find(conversation => conversation.id === state.currentId)

  const send = useCallback((text: string) => {
    const content = text.trim()
    if (bridge === null || content === '') return
    const time = Date.now()
    const base = current ?? { id: crypto.randomUUID(), title: content.split('\n', 1)[0]!.slice(0, 24), updatedAt: time, messages: [] }
    const history: ChatMessage[] = [...base.messages, { role: 'user', content, time }]
    dispatch({ type: 'send', conversation: { ...base, updatedAt: time, messages: [...history, { role: 'assistant', content: '' }] } })
    bridge.send(base.id, history)
  }, [bridge, current])

  const open = useCallback((id: string | null) => {
    dispatch({ type: 'open', id })
  }, [])

  const rename = useCallback((id: string, title: string) => {
    dispatch({ type: 'rename', id, title })
  }, [])

  const remove = useCallback((ids: string[]) => {
    for (const id of ids) bridge?.forget(id)
    dispatch({ type: 'remove', ids })
  }, [bridge])

  const stop = useCallback(() => {
    bridge?.cancel()
  }, [bridge])

  return {
    conversations: state.conversations,
    current,
    running: current !== undefined && state.running === current.id,
    open,
    rename,
    remove,
    send: bridge === null ? undefined : send,
    stop,
  }
}
