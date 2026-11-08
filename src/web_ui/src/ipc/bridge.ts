/*
 * IPC with the native side (src/chat/chatbridge.{h,cpp}) over QWebChannel.
 *
 * The host injects qwebchannel.js and sets window.gxdeBridge before any page
 * script runs.  Outside the host (a plain browser on `npm run dev`) there is
 * no bridge and connectBridge() resolves to null.
 */

export type Role = 'system' | 'user' | 'assistant'

export type ReplyPart =
  | { type: 'text'; text: string }
  | { type: 'reasoning'; text: string; startedAt: number; endedAt?: number }
  | { type: 'tool'; id: string; title: string; kind: string; status: ToolCallStatus; output?: ToolOutput[] }

/** What a tool call showed: its text, or an edit as unified diff lines. */
export type ToolOutput =
  | { type: 'text'; text: string }
  | { type: 'diff'; path: string; lines: string[] }

export interface ChatMessage {
  role: Role
  /** For a reply, its text alone: this is what the backend replays as history. */
  content: string
  /** Unix epoch ms: sent for the user's, finished for the assistant's. */
  time?: number
  /** A reply's text, reasoning and tool calls in the order they streamed. */
  parts?: ReplyPart[]
  /** Why a reply ended other than end_turn. */
  stopReason?: StopReason
  error?: string
}

/** An AI Core provider account and one of its models. */
export interface Provider {
  accountId: string
  model: string
}

export interface Conversation {
  id: string
  title: string
  updatedAt: number
  messages: ChatMessage[]
  /** Unset: AI Core's default account. */
  provider?: Provider
}

/** chat.json, shared with the earlier native window. */
export interface ConversationStore {
  version?: number
  current?: number
  conversations?: Conversation[]
}

export interface ChatMode {
  id: string
  label: string
}

export type ToolCallStatus = 'pending' | 'in_progress' | 'completed' | 'failed'

/** ACP PermissionOption */
export interface PermissionOption {
  optionId: string
  name: string
  kind: 'allow_once' | 'allow_always' | 'reject_once' | 'reject_always'
}

/** ACP ToolCall, as far as the UI reads it */
export interface ToolCall {
  toolCallId: string
  title?: string
  kind?: string
  content?: unknown[]
  locations?: { path: string }[]
  [key: string]: unknown
}

export type StopReason =
  | 'end_turn'
  | 'max_tokens'
  | 'max_turn_requests'
  | 'refusal'
  | 'cancelled'
  | (string & {})

/** Native → page notifications, keyed by signal name. */
export interface BridgeEvents {
  busyChanged: []
  modeChanged: []
  delta: [conversationId: string, text: string]
  thought: [conversationId: string, text: string]
  toolCall: [
    conversationId: string,
    toolCallId: string,
    title: string,
    kind: string,
    status: ToolCallStatus,
    /** ACP ToolCallContent; null when the update left it unchanged. */
    content: unknown[] | null,
  ]
  permissionRequested: [
    conversationId: string,
    key: string,
    toolCall: ToolCall,
    options: PermissionOption[],
  ]
  permissionDropped: [key: string]
  finished: [conversationId: string, stopReason: StopReason]
  failed: [conversationId: string, error: string]
  activity: [conversationId: string]
  accountsChanged: []
  askRequested: [text: string, files: string[]]
  turnRevealRequested: []
  coreRestarted: []
  /** Drops the page cannot read, such as local files, at page coordinates. */
  urlsDropped: [urls: string[], x: number, y: number]
}

/** Chat settings kept outside AI Core. */
export interface ChatSettings {
  /** JSON array text, as the user wrote it. */
  mcpServers: string
  systemPrompt: string
}

/* ---- the raw QWebChannel object ---- */

interface QtSignal<A extends unknown[]> {
  connect(callback: (...args: A) => void): void
  disconnect(callback: (...args: A) => void): void
}

/** Every invokable takes an optional trailing callback for its return value. */
type QtMethod<A extends unknown[], R = void> = (...args: [...A, ((result: R) => void)?]) => void

type RawSignals = { [K in keyof BridgeEvents]: QtSignal<BridgeEvents[K]> }

interface RawBridge extends RawSignals {
  readonly busy: boolean
  mode: string
  replied: QtSignal<[callId: number, result: unknown, error: string]>

  ready: QtMethod<[]>
  modes: QtMethod<[], ChatMode[]>
  send: QtMethod<[conversationId: string, history: ChatMessage[], providerAccountId: string, model: string]>
  cancel: QtMethod<[]>
  forget: QtMethod<[conversationId: string]>
  resetSessions: QtMethod<[]>
  answerPermission: QtMethod<[key: string, optionId: string]>
  loadConversations: QtMethod<[], ConversationStore>
  saveConversations: QtMethod<[store: ConversationStore], boolean>
  listAccounts: QtMethod<[callId: number]>
  currentAccount: QtMethod<[callId: number]>
  saveAccount: QtMethod<[callId: number, account: object, apiKey: string]>
  deleteAccount: QtMethod<[callId: number, id: string]>
  activateAccount: QtMethod<[callId: number, id: string, model: string]>
  settings: QtMethod<[], ChatSettings>
  saveSettings: QtMethod<[settings: ChatSettings], string>
  saveImage: QtMethod<[base64: string], string>
}

declare global {
  interface Window {
    gxdeBridge?: Promise<RawBridge>
  }
}

/* ---- typed client ---- */

export class Bridge {
  private readonly raw: RawBridge
  private nextCallId = 1
  private readonly pending = new Map<
    number,
    { resolve: (value: unknown) => void; reject: (error: Error) => void }
  >()

  constructor(raw: RawBridge) {
    this.raw = raw
    raw.replied.connect((callId, result, error) => {
      const call = this.pending.get(callId)
      if (!call) return
      this.pending.delete(callId)
      if (error) call.reject(new Error(error))
      else call.resolve(result)
    })
  }

  /** Subscribe to a native signal; returns the unsubscribe function. */
  on<K extends keyof BridgeEvents>(
    event: K,
    callback: (...args: BridgeEvents[K]) => void,
  ): () => void {
    const signal = this.raw[event] as QtSignal<BridgeEvents[K]>
    signal.connect(callback)
    return () => signal.disconnect(callback)
  }

  /**
   * Tell the host the page is listening; queued askRequested events follow.
   * Call once, after subscribing to askRequested.
   */
  ready(): void {
    this.raw.ready()
  }

  get busy(): boolean {
    return this.raw.busy
  }

  get mode(): string {
    return this.raw.mode
  }

  set mode(modeId: string) {
    this.raw.mode = modeId
  }

  setMode(modeId: string): void {
    this.raw.mode = modeId
  }

  modes(): Promise<ChatMode[]> {
    return this.invoke(this.raw.modes)
  }

  /**
   * Answer the last message of history, which must be the user's.  Replies
   * stream in as delta / thought / toolCall and end with finished or failed.
   */
  send(conversationId: string, history: ChatMessage[], provider?: Provider): void {
    this.raw.send(conversationId, history, provider?.accountId ?? '', provider?.model ?? '')
  }

  cancel(): void {
    this.raw.cancel()
  }

  forget(conversationId: string): void {
    this.raw.forget(conversationId)
  }

  resetSessions(): void {
    this.raw.resetSessions()
  }

  /** An empty optionId rejects. */
  answerPermission(key: string, optionId: string): void {
    this.raw.answerPermission(key, optionId)
  }

  loadConversations(): Promise<ConversationStore> {
    return this.invoke(this.raw.loadConversations)
  }

  saveConversations(store: ConversationStore): Promise<boolean> {
    return this.invoke(this.raw.saveConversations, store)
  }

  /* AI Core provider accounts: shapes are AI Core's own JSON */

  listAccounts(): Promise<unknown> {
    return this.request((id) => this.raw.listAccounts(id))
  }

  currentAccount(): Promise<unknown> {
    return this.request((id) => this.raw.currentAccount(id))
  }

  /** An empty apiKey keeps the stored key of an existing account. */
  saveAccount(account: object, apiKey = ''): Promise<unknown> {
    return this.request((id) => this.raw.saveAccount(id, account, apiKey))
  }

  deleteAccount(accountId: string): Promise<unknown> {
    return this.request((id) => this.raw.deleteAccount(id, accountId))
  }

  activateAccount(accountId: string, model: string): Promise<unknown> {
    return this.request((id) => this.raw.activateAccount(id, accountId, model))
  }

  settings(): Promise<ChatSettings> {
    return this.invoke(this.raw.settings)
  }

  /** Resolves to why nothing was saved, or '' on success. */
  saveSettings(settings: ChatSettings): Promise<string> {
    return this.invoke(this.raw.saveSettings, settings)
  }

  /** Store image bytes (base64) in the cache; resolves to the path, '' on failure. */
  saveImage(base64: string): Promise<string> {
    return this.invoke(this.raw.saveImage, base64)
  }

  /** Methods that return their value directly (through the QWebChannel callback). */
  private invoke<A extends unknown[], R>(method: QtMethod<A, R>, ...args: A): Promise<R> {
    return new Promise((resolve) => {
      ;(method as (...a: unknown[]) => void).apply(this.raw, [...args, resolve])
    })
  }

  /** Methods that answer later with replied(callId, …). */
  private request(start: (callId: number) => void): Promise<unknown> {
    const callId = this.nextCallId++
    return new Promise((resolve, reject) => {
      this.pending.set(callId, { resolve, reject })
      start(callId)
    })
  }
}

let connection: Promise<Bridge | null> | undefined

/** The bridge to the native side, or null when not running inside the host. */
export function connectBridge(): Promise<Bridge | null> {
  connection ??= window.gxdeBridge
    ? window.gxdeBridge.then((raw) => new Bridge(raw))
    : Promise.resolve(null)
  return connection
}
