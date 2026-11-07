/*
 * A stand-in for the native bridge on `npm run dev` in a plain browser.
 * Replies are canned: they exercise streaming, reasoning, tool calls and
 * confirmations, not a model.  window.gxdeDev.emit() fires host-only signals.
 */
import type { BridgeEvents, ChatMessage, ChatSettings, ConversationStore } from './bridge'

type Listener = (...args: unknown[]) => void
type Callback = ((result: unknown) => void) | undefined

const STORE = 'gxde-dev-chat'
const ACCOUNTS = 'gxde-dev-accounts'
const SETTINGS = 'gxde-dev-settings'

const MODES = [
  { id: 'default', label: '每次确认' },
  { id: 'autoEdit', label: '自动允许编辑' },
  { id: 'yolo', label: '全部自动允许' },
  { id: 'plan', label: '只读' },
]

const PRESETS = [{
  id: 'deepseek',
  label: 'DeepSeek',
  protocol: 'openai',
  baseUrl: [{ url: 'https://api.deepseek.com', label: '中国大陆', documentationUrl: 'https://platform.deepseek.com/api_keys' }],
  models: [{ id: 'deepseek-flash' }, { id: 'deepseek-v4-pro' }],
}]

interface AccountState {
  accounts: Record<string, unknown>[]
  activeAccountId?: string
}

function read<T>(key: string, fallback: T): T {
  try {
    const text = localStorage.getItem(key)
    return text === null ? fallback : JSON.parse(text) as T
  }
  catch {
    return fallback
  }
}

function write(key: string, value: unknown) {
  try {
    localStorage.setItem(key, JSON.stringify(value))
  }
  catch {
    // Private windows may refuse; the session still works.
  }
}

const sleep = (ms: number) => new Promise(resolve => setTimeout(resolve, ms))

export function installDevBridge() {
  const listeners = new Map<string, Set<Listener>>()
  const emit = (name: string, ...args: unknown[]) => {
    for (const listener of listeners.get(name) ?? []) listener(...args)
  }
  const signal = (name: string) => ({
    connect: (listener: Listener) => {
      if (!listeners.has(name)) listeners.set(name, new Set())
      listeners.get(name)!.add(listener)
    },
    disconnect: (listener: Listener) => {
      listeners.get(name)?.delete(listener)
    },
  })

  let mode = 'default'
  let turn: { id: string; cancelled: boolean } | null = null
  const answers = new Map<string, (optionId: string) => void>()
  const asks: [string, string[]][] = []
  let ready = false
  const accounts = () => read<AccountState>(ACCOUNTS, { accounts: [] })
  const reply = (callId: number, work: () => unknown) => {
    setTimeout(() => {
      try {
        emit('replied', callId, work(), '')
      }
      catch (error) {
        emit('replied', callId, null, error instanceof Error ? error.message : String(error))
      }
    }, 150)
  }

  async function run(id: string, history: ChatMessage[], answeredBy: string) {
    const self = { id, cancelled: false }
    turn = self
    const question = history.at(-1)?.content ?? ''
    const alive = () => turn === self && !self.cancelled
    for (const piece of ['开发模式：', '没有连接 GXDE AI Core，', '这是一段模拟的思考。']) {
      await sleep(250)
      if (!alive()) return
      emit('thought', id, piece)
      emit('activity', id)
    }
    const toolId = crypto.randomUUID()
    emit('toolCall', id, toolId, `cat ${question.split('\n')[0]!.slice(0, 20) || 'README.md'}`, 'read', 'pending')
    if (mode === 'default') {
      const key = crypto.randomUUID()
      emit('permissionRequested', id, key, { toolCallId: toolId, title: '模拟工具调用', kind: 'execute', content: [{ type: 'content', content: { type: 'text', text: `echo ${JSON.stringify(question.slice(0, 40))}` } }] }, [
        { optionId: 'allow', name: '允许', kind: 'allow_once' },
        { optionId: 'always', name: '总是允许', kind: 'allow_always' },
        { optionId: 'reject', name: '拒绝', kind: 'reject_once' },
      ])
      const answer = await new Promise<string>((resolve) => {
        answers.set(key, resolve)
      })
      answers.delete(key)
      if (!alive()) return
      emit('toolCall', id, toolId, '', '', answer === 'reject' || answer === '' ? 'failed' : 'completed')
    }
    else {
      emit('toolCall', id, toolId, '', '', 'in_progress')
      await sleep(400)
      if (!alive()) return
      emit('toolCall', id, toolId, '', '', mode === 'plan' ? 'failed' : 'completed')
    }
    const text = `开发模式的模拟回复，没有经过模型（所选：${answeredBy}）。\n\n你说的是：\n\n> ${question.replace(/\n/g, '\n> ')}\n\n\`\`\`ts\nconsole.log('dev bridge')\n\`\`\``
    for (let at = 0; at < text.length; at += 6) {
      await sleep(30)
      if (!alive()) return
      emit('delta', id, text.slice(at, at + 6))
    }
    turn = null
    emit('busyChanged')
    emit('finished', id, 'end_turn')
  }

  const raw = {
    get busy() {
      return turn !== null
    },
    get mode() {
      return mode
    },
    set mode(next: string) {
      mode = next
      emit('modeChanged')
    },
    ready: () => {
      ready = true
      for (const [text, files] of asks.splice(0)) emit('askRequested', text, files)
    },
    modes: (done: Callback) => done?.(MODES),
    send: (id: string, history: ChatMessage[], accountId: string, model: string) => {
      emit('busyChanged')
      const state = accounts()
      const account = state.accounts.find(item => item.id === (accountId || state.activeAccountId))
      void run(id, history, `${String(account?.name ?? '?')} · ${model || String(account?.model ?? '?')}`)
    },
    cancel: () => {
      if (turn === null) return
      const { id } = turn
      turn.cancelled = true
      turn = null
      for (const [key, answer] of answers) {
        emit('permissionDropped', key)
        answer('')
      }
      emit('busyChanged')
      emit('finished', id, 'cancelled')
    },
    forget: () => undefined,
    resetSessions: () => undefined,
    answerPermission: (key: string, optionId: string) => answers.get(key)?.(optionId),
    loadConversations: (done: Callback) => done?.(read<ConversationStore>(STORE, { version: 1, conversations: [] })),
    saveConversations: (store: ConversationStore, done: Callback) => {
      write(STORE, store)
      done?.(true)
    },
    listAccounts: (callId: number) => {
      reply(callId, () => ({ presets: PRESETS, ...accounts() }))
    },
    currentAccount: (callId: number) => {
      reply(callId, () => {
        const state = accounts()
        return { account: state.accounts.find(account => account.id === state.activeAccountId) ?? null }
      })
    },
    saveAccount: (callId: number, account: Record<string, unknown>) => {
      reply(callId, () => {
        const state = accounts()
        const saved = { ...account, id: account.id ?? crypto.randomUUID() }
        const others = state.accounts.filter(item => item.id !== saved.id)
        write(ACCOUNTS, { ...state, accounts: [...others, saved] })
        setTimeout(() => emit('accountsChanged'))
        return { account: saved }
      })
    },
    deleteAccount: (callId: number, id: string) => {
      reply(callId, () => {
        const state = accounts()
        if (id === state.activeAccountId) throw new Error('Cannot delete the active account')
        write(ACCOUNTS, { ...state, accounts: state.accounts.filter(item => item.id !== id) })
        setTimeout(() => emit('accountsChanged'))
        return {}
      })
    },
    activateAccount: (callId: number, id: string, model: string) => {
      reply(callId, () => {
        const state = accounts()
        const list = state.accounts.map(item => item.id === id && model !== '' ? { ...item, model } : item)
        write(ACCOUNTS, { accounts: list, activeAccountId: id })
        setTimeout(() => emit('accountsChanged'))
        return {}
      })
    },
    settings: (done: Callback) => done?.(read<ChatSettings>(SETTINGS, { mcpServers: '', systemPrompt: '' })),
    saveSettings: (settings: ChatSettings, done: Callback) => {
      const mcp = settings.mcpServers.trim()
      let valid = mcp === ''
      try {
        valid ||= Array.isArray(JSON.parse(mcp))
      }
      catch {
        valid = false
      }
      if (valid) write(SETTINGS, { ...settings, mcpServers: mcp })
      done?.(valid ? '' : 'MCP 配置不是合法的 JSON 数组')
    },
    saveImage: (_base64: string, done: Callback) => done?.(`/tmp/gxde-dev-${crypto.randomUUID()}.png`),
  }
  const signals = [
    'busyChanged', 'modeChanged', 'delta', 'thought', 'toolCall', 'permissionRequested', 'permissionDropped', 'finished',
    'failed', 'activity', 'accountsChanged', 'askRequested', 'turnRevealRequested', 'coreRestarted', 'urlsDropped', 'replied',
  ] satisfies (keyof BridgeEvents | 'replied')[]
  for (const name of signals) Object.defineProperty(raw, name, { value: signal(name) })

  window.gxdeBridge = Promise.resolve(raw as unknown as NonNullable<Window['gxdeBridge']> extends Promise<infer R> ? R : never)
  Object.assign(window, {
    gxdeDev: {
      emit: <K extends keyof BridgeEvents>(name: K, ...args: BridgeEvents[K]) => {
        if (name === 'askRequested' && !ready) asks.push(args as [string, string[]])
        else emit(name, ...args)
      },
    },
  })
}
