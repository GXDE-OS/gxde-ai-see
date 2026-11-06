import { useCallback, useEffect, useReducer, useRef } from 'react'
import type {
  Bridge, ChatMessage, Conversation, ConversationStore, PermissionOption, Provider, StopReason, ToolCall, ToolCallStatus,
} from '../ipc/bridge'
import { appendReasoning, appendText, closeReply, updateTool } from './reply'
import { t } from './strings'

export interface Approval {
  key: string
  conversationId: string
  toolCall: ToolCall
  options: PermissionOption[]
}

interface State {
  loaded: boolean
  conversations: Conversation[]
  currentId: string | null
  running: string | null
  /** Last sign of life from the running turn. */
  activityAt: number
  approvals: Approval[]
  /** The last pick, for conversations not started yet. */
  draftProvider: Provider | undefined
  /** Bumped by every change worth writing to chat.json. */
  revision: number
}

type Action =
  | { type: 'load'; store: ConversationStore }
  | { type: 'open'; id: string | null }
  | { type: 'reveal' }
  | { type: 'provider'; provider: Provider }
  | { type: 'rename'; id: string; title: string }
  | { type: 'remove'; ids: string[] }
  | { type: 'add'; conversation: Conversation }
  | { type: 'start'; conversation: Conversation }
  | { type: 'stream'; id: string; time: number; update: (reply: ChatMessage, time: number) => ChatMessage }
  | { type: 'activity'; id: string; time: number }
  | { type: 'ask'; approval: Approval }
  | { type: 'drop'; key: string; time: number }
  | { type: 'end'; id: string; time: number; stopReason?: StopReason; error?: string }

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
    case 'reveal':
      return { ...state, currentId: state.running ?? state.currentId }
    case 'provider':
      if (state.currentId === null) return { ...state, draftProvider: action.provider }
      return {
        ...state,
        draftProvider: action.provider,
        conversations: state.conversations.map(conversation =>
          conversation.id === state.currentId ? { ...conversation, provider: action.provider } : conversation),
        revision: state.revision + 1,
      }
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
    case 'add':
    case 'start': {
      const others = state.conversations.filter(conversation => conversation.id !== action.conversation.id)
      const next = {
        ...state,
        conversations: [action.conversation, ...others],
        currentId: action.conversation.id,
        revision: state.revision + 1,
      }
      return action.type === 'add' ? next : { ...next, running: action.conversation.id, activityAt: action.conversation.updatedAt }
    }
    case 'stream':
      if (state.running !== action.id) return state
      return { ...state, conversations: updateReply(state, action.id, reply => action.update(reply, action.time)) }
    case 'activity':
      return state.running === action.id ? { ...state, activityAt: action.time } : state
    case 'ask':
      return { ...state, approvals: [...state.approvals, action.approval] }
    case 'drop':
      // Time spent waiting on the user is not the model going quiet.
      return { ...state, approvals: state.approvals.filter(approval => approval.key !== action.key), activityAt: action.time }
    case 'end': {
      const conversations = updateReply(state, action.id, reply =>
        closeReply(reply, action.time, action.stopReason, action.error)).map(conversation => conversation.id === action.id ? { ...conversation, updatedAt: action.time } : conversation)
      return {
        ...state,
        conversations: conversations.sort(byRecent),
        running: state.running === action.id ? null : state.running,
        approvals: state.approvals.filter(approval => approval.conversationId !== action.id),
        revision: state.revision + 1,
      }
    }
  }
}

const INITIAL: State = {
  loaded: false, conversations: [], currentId: null, running: null, activityAt: 0, approvals: [], draftProvider: undefined, revision: 0,
}

export interface Chat {
  conversations: Conversation[]
  current: Conversation | undefined
  /** The conversation whose turn is streaming, wherever it is. */
  runningId: string | null
  /** The current conversation's turn is streaming. */
  running: boolean
  activityAt: number
  approvals: Approval[]
  /** The current conversation's pick, or the one a new conversation will take. */
  provider: Provider | undefined
  setProvider: (provider: Provider) => void
  open: (id: string | null) => void
  rename: (id: string, title: string) => void
  remove: (ids: string[]) => void
  /** Undefined outside the host. */
  send: ((text: string, title?: string) => void) | undefined
  /** Answer the user message before index again. */
  retry: (index: number) => void
  /** Replace the user message at index and answer from there. */
  edit: (index: number, text: string) => void
  /** Copy the conversation up to index into a new one. */
  fork: (index: number) => void
  answer: (key: string, optionId: string) => void
  stop: () => void
}

export function useChat(bridge: Bridge | null): Chat {
  const [state, dispatch] = useReducer(reduce, INITIAL)

  useEffect(() => {
    if (bridge === null) return
    let alive = true
    const now = Date.now
    const unsubscribe = [
      bridge.on('delta', (id, text) => {
        dispatch({ type: 'stream', id, time: now(), update: (reply, time) => appendText(reply, text, time) })
      }),
      bridge.on('thought', (id, text) => {
        dispatch({ type: 'stream', id, time: now(), update: (reply, time) => appendReasoning(reply, text, time) })
      }),
      bridge.on('toolCall', (id, toolCallId, title, kind, status: ToolCallStatus) => {
        dispatch({ type: 'stream', id, time: now(), update: (reply, time) => updateTool(reply, toolCallId, title, kind, status, time) })
      }),
      bridge.on('activity', (id) => {
        dispatch({ type: 'activity', id, time: now() })
      }),
      bridge.on('permissionRequested', (conversationId, key, toolCall, options) => {
        dispatch({ type: 'ask', approval: { key, conversationId, toolCall, options } })
      }),
      bridge.on('permissionDropped', (key) => {
        dispatch({ type: 'drop', key, time: now() })
      }),
      bridge.on('finished', (id, stopReason) => {
        dispatch({ type: 'end', id, time: now(), stopReason })
      }),
      bridge.on('failed', (id, error) => {
        dispatch({ type: 'end', id, time: now(), error })
      }),
      bridge.on('turnRevealRequested', () => {
        dispatch({ type: 'reveal' })
      }),
    ]
    void bridge.loadConversations().then((store) => {
      if (alive) dispatch({ type: 'load', store })
    })
    return () => {
      alive = false
      for (const off of unsubscribe) off()
    }
  }, [bridge])

  const saved = useRef(0)
  useEffect(() => {
    if (bridge === null || !state.loaded || state.revision === saved.current) return
    saved.current = state.revision
    void bridge.saveConversations({ version: 1, conversations: state.conversations })
  }, [bridge, state])

  const current = state.conversations.find(conversation => conversation.id === state.currentId)

  // Answers the last message of history, which must be the user's.
  const start = useCallback((base: Conversation, history: ChatMessage[]) => {
    if (bridge === null) return
    const time = Date.now()
    dispatch({ type: 'start', conversation: { ...base, updatedAt: time, messages: [...history, { role: 'assistant', content: '' }] } })
    bridge.send(base.id, history.map(({ role, content }) => ({ role, content })), base.provider)
  }, [bridge])

  const send = useCallback((text: string, title?: string) => {
    const content = text.trim()
    if (content === '') return
    const time = Date.now()
    const base: Conversation = current ?? {
      id: crypto.randomUUID(),
      title: title ?? content.split('\n', 1)[0]!.slice(0, 24),
      updatedAt: time,
      messages: [],
      ...(state.draftProvider === undefined ? {} : { provider: state.draftProvider }),
    }
    start(base, [...base.messages, { role: 'user', content, time }])
  }, [current, start, state.draftProvider])

  const retry = useCallback((index: number) => {
    if (current === undefined || state.running !== null) return
    const history = current.messages.slice(0, index)
    if (history.at(-1)?.role === 'user') start(current, history)
  }, [current, start, state.running])

  const edit = useCallback((index: number, text: string) => {
    const content = text.trim()
    if (current === undefined || state.running !== null || content === '') return
    start(current, [...current.messages.slice(0, index), { role: 'user', content, time: Date.now() }])
  }, [current, start, state.running])

  const fork = useCallback((index: number) => {
    if (current === undefined) return
    dispatch({
      type: 'add',
      conversation: {
        id: crypto.randomUUID(),
        title: t.forkTitle(current.title || t.untitled),
        updatedAt: Date.now(),
        messages: current.messages.slice(0, index + 1),
        ...(current.provider === undefined ? {} : { provider: current.provider }),
      },
    })
  }, [current])

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

  const answer = useCallback((key: string, optionId: string) => {
    bridge?.answerPermission(key, optionId)
    dispatch({ type: 'drop', key, time: Date.now() })
  }, [bridge])

  const stop = useCallback(() => {
    bridge?.cancel()
  }, [bridge])

  const setProvider = useCallback((provider: Provider) => {
    dispatch({ type: 'provider', provider })
  }, [])

  return {
    conversations: state.conversations,
    current,
    runningId: state.running,
    running: current !== undefined && state.running === current.id,
    activityAt: state.activityAt,
    approvals: state.approvals,
    provider: current === undefined ? state.draftProvider : current.provider,
    setProvider,
    open,
    rename,
    remove,
    send: bridge === null ? undefined : send,
    retry,
    edit,
    fork,
    answer,
    stop,
  }
}
