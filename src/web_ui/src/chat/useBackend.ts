import { useCallback, useEffect, useState } from 'react'
import type { Bridge, ChatMode } from '../ipc/bridge'

/* AI Core's account JSON, as far as the UI reads and writes it. */

export interface ModelInfo {
  id: string
  [key: string]: unknown
}

export interface Account {
  id?: string
  name: string
  providerId: string
  protocol: string
  baseUrl: string
  models: ModelInfo[]
  model: string
}

export interface Endpoint {
  url: string
  label?: string
  documentationUrl?: string
}

export interface Preset {
  id: string
  label: string
  protocol?: string
  /** One URL, or regional endpoints. */
  baseUrl?: string | Endpoint[]
  models?: ModelInfo[]
}

export interface AccountList {
  presets: Preset[]
  accounts: Account[]
  activeAccountId?: string
}

export interface Accounts {
  /** Undefined until AI Core answers. */
  list: AccountList | undefined
  /** Why AI Core could not be asked, if it could not. */
  error: string | null
  refresh: () => void
}

/** AI Core's provider accounts, refreshed whenever they change. */
export function useAccounts(bridge: Bridge | null): Accounts {
  const [list, setList] = useState<AccountList | undefined>(undefined)
  const [error, setError] = useState<string | null>(null)
  const [generation, setGeneration] = useState(0)
  const refresh = useCallback(() => {
    setGeneration(n => n + 1)
  }, [])

  useEffect(() => {
    if (bridge === null) return
    return bridge.on('accountsChanged', refresh)
  }, [bridge, refresh])

  useEffect(() => {
    if (bridge === null) return
    let alive = true
    bridge.listAccounts().then((result) => {
      if (!alive) return
      const next = result as Partial<AccountList> | null
      setList({
        presets: next?.presets ?? [],
        accounts: next?.accounts ?? [],
        ...(next?.activeAccountId ? { activeAccountId: next.activeAccountId } : {}),
      })
      setError(null)
    }, (reason: unknown) => {
      if (!alive) return
      setList({ presets: [], accounts: [] })
      setError(reason instanceof Error ? reason.message : String(reason))
    })
    return () => {
      alive = false
    }
  }, [bridge, generation])

  return { list, error, refresh }
}

export interface Modes {
  modes: ChatMode[]
  mode: string
  setMode: (id: string) => void
}

export function useModes(bridge: Bridge | null): Modes {
  const [modes, setModes] = useState<ChatMode[]>([])
  const [mode, setModeState] = useState('')
  useEffect(() => {
    if (bridge === null) return
    let alive = true
    void bridge.modes().then((list) => {
      if (!alive) return
      setModes(list)
      setModeState(bridge.mode)
    })
    const off = bridge.on('modeChanged', () => {
      setModeState(bridge.mode)
    })
    return () => {
      alive = false
      off()
    }
  }, [bridge])
  const setMode = useCallback((id: string) => {
    bridge?.setMode(id)
  }, [bridge])
  return { modes, mode, setMode }
}
