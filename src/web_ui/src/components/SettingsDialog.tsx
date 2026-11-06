/*
 * Form fields reuse DeepSeek Harness ui-primitives ConfigField styling
 * (MIT, see src/vendor/dsh/README.md).
 */
import { useEffect, useId, useState } from 'react'
import type { ReactNode } from 'react'
import clsx from 'clsx'
import { t } from '../chat/strings'
import type { Account, Accounts as AccountsSource, Endpoint, ModelInfo, Preset } from '../chat/useBackend'
import type { Bridge } from '../ipc/bridge'
import { Button } from '../vendor/dsh/primitives/Button.tsx'
import { Modal } from '../vendor/dsh/primitives/Modal.tsx'
import field from '../vendor/dsh/primitives/ConfigField.module.css'
import css from './SettingsDialog.module.css'

const CUSTOM = 'custom'
const MCP_EXAMPLE = '[{"name":"fs","command":"npx","args":["-y","@modelcontextprotocol/server-filesystem","/tmp"]}]'
const PROTOCOLS = ['openai', 'anthropic', 'gemini']

const messageOf = (reason: unknown) => reason instanceof Error ? reason.message : String(reason)

function endpointsOf(preset: Preset | undefined): Endpoint[] {
  const base = preset?.baseUrl
  if (Array.isArray(base)) return base
  return typeof base === 'string' ? [{ url: base }] : []
}

const idsOf = (models: readonly ModelInfo[] | undefined) => (models ?? []).map(model => model.id).filter(id => id !== '')

function Field({ label, children }: { label: string; children: (id: string) => ReactNode }) {
  const id = useId()
  return (
    <div className={field.field}>
      <div className={field.head}><label htmlFor={id}>{label}</label></div>
      {children(id)}
    </div>
  )
}

function validUrl(text: string): boolean {
  try {
    const url = new URL(text)
    return (url.protocol === 'https:' || url.protocol === 'http:') && url.hostname !== '' && url.search === '' && url.hash === ''
  }
  catch {
    return false
  }
}

function AccountEditor({ presets, original, onCancel, onSave }: {
  presets: Preset[]
  /** Undefined adds a new account. */
  original: Account | undefined
  onCancel: () => void
  onSave: (account: Account, apiKey: string) => void
}) {
  const editing = original !== undefined
  const initialPreset = editing
    ? (presets.some(preset => preset.id === original.providerId) ? original.providerId : CUSTOM)
    : (presets.some(preset => preset.id === 'deepseek') ? 'deepseek' : (presets[0]?.id ?? CUSTOM))
  const presetOf = (id: string) => presets.find(preset => preset.id === id)
  const [presetId, setPresetId] = useState(initialPreset)
  const [protocol, setProtocol] = useState(original?.protocol ?? presetOf(initialPreset)?.protocol ?? 'openai')
  const [baseUrl, setBaseUrl] = useState(original?.baseUrl ?? endpointsOf(presetOf(initialPreset))[0]?.url ?? '')
  const [name, setName] = useState(original?.name ?? presetOf(initialPreset)?.label ?? '')
  const [apiKey, setApiKey] = useState('')
  const [models, setModels] = useState((original ? idsOf(original.models) : idsOf(presetOf(initialPreset)?.models)).join(', '))
  const [model, setModel] = useState(original?.model ?? '')
  const [problem, setProblem] = useState('')

  const preset = presetOf(presetId)
  const endpoints = endpointsOf(preset)
  const endpoint = endpoints.find(item => item.url === baseUrl)
  const modelIds = [...new Set(models.split(',').map(id => id.trim()).filter(id => id !== ''))]
  const defaultModel = modelIds.includes(model) ? model : (modelIds[0] ?? '')

  const choosePreset = (id: string) => {
    const next = presetOf(id)
    setPresetId(id)
    if (next !== undefined) setProtocol(next.protocol ?? 'openai')
    setBaseUrl(endpointsOf(next)[0]?.url ?? '')
    if (!editing || name === '') setName(next?.label ?? '')
    setModels(idsOf(next?.models).join(', '))
  }

  const save = () => {
    const known = [...(original?.models ?? []), ...(preset?.models ?? [])]
    const account: Account = {
      ...(editing && original.id !== undefined ? { id: original.id } : {}),
      name: name.trim(),
      providerId: preset === undefined ? CUSTOM : preset.id,
      protocol,
      baseUrl: baseUrl.trim(),
      // Keep what AI Core knows about a model (context window, reasoning …).
      models: modelIds.map(id => known.find(info => info.id === id) ?? { id }),
      model: defaultModel,
    }
    const issue = account.name === '' ? t.needName
      : !validUrl(account.baseUrl) ? t.badUrl
        : !editing && apiKey.trim() === '' ? t.needKey
          : modelIds.length === 0 ? t.needModel
            : !modelIds.includes(account.model) ? t.badDefaultModel
              : ''
    setProblem(issue)
    if (issue === '') onSave(account, apiKey.trim())
  }

  return (
    <Modal
      open
      onClose={onCancel}
      closeLabel={t.close}
      title={editing ? t.editAccount : t.addAccount}
      className={css.dialog}
      contentClassName={css.content}
      footer={(
        <>
          <Button variant="outline" onClick={onCancel}>{t.cancel}</Button>
          <Button variant="primary" onClick={save}>{t.save}</Button>
        </>
      )}
    >
      <Field label={t.provider}>
        {id => (
          <select id={id} value={presetId} data-modal-autofocus onChange={(event) => { choosePreset(event.target.value) }}>
            {presets.map(item => <option key={item.id} value={item.id}>{item.label}</option>)}
            <option value={CUSTOM}>{t.customProviderLong}</option>
          </select>
        )}
      </Field>
      {endpoints.length > 1 && (
        <Field label={t.endpoint}>
          {id => (
            <select id={id} value={endpoint?.url ?? ''} onChange={(event) => { setBaseUrl(event.target.value) }}>
              {endpoint === undefined && <option value="" disabled>{baseUrl}</option>}
              {endpoints.map(item => <option key={item.url} value={item.url}>{item.label ?? item.url}</option>)}
            </select>
          )}
        </Field>
      )}
      <Field label={t.protocol}>
        {id => (
          <select id={id} value={protocol} disabled={preset !== undefined} onChange={(event) => { setProtocol(event.target.value) }}>
            {PROTOCOLS.map(item => <option key={item} value={item}>{t.protocols[item]}</option>)}
          </select>
        )}
      </Field>
      <Field label={t.baseUrl}>
        {id => <input id={id} value={baseUrl} placeholder="https://api.example.com/v1" onChange={(event) => { setBaseUrl(event.target.value) }} />}
      </Field>
      <Field label={t.accountName}>
        {id => <input id={id} value={name} placeholder={t.accountNamePlaceholder} onChange={(event) => { setName(event.target.value) }} />}
      </Field>
      <Field label={t.apiKey}>
        {id => (
          <input
            id={id}
            type="password"
            autoComplete="new-password"
            value={apiKey}
            placeholder={editing ? t.keepKey : 'sk-…'}
            onChange={(event) => { setApiKey(event.target.value) }}
          />
        )}
      </Field>
      <Field label={t.modelList}>
        {id => <input id={id} value={models} placeholder={t.modelListPlaceholder} onChange={(event) => { setModels(event.target.value) }} />}
      </Field>
      <Field label={t.defaultModel}>
        {id => (
          <select id={id} value={defaultModel} disabled={modelIds.length === 0} onChange={(event) => { setModel(event.target.value) }}>
            {modelIds.map(item => <option key={item} value={item}>{item}</option>)}
          </select>
        )}
      </Field>
      {endpoint?.documentationUrl !== undefined && endpoint.documentationUrl !== '' && (
        <a className={css.link} href={endpoint.documentationUrl} target="_blank" rel="noreferrer">{t.keyDocs}</a>
      )}
      <p className={css.hint}>{t.keyringHint}</p>
      {problem !== '' && <p className={css.error} role="alert">{problem}</p>}
    </Modal>
  )
}

function Accounts({ bridge, source }: { bridge: Bridge; source: AccountsSource }) {
  const { list } = source
  const [selected, setSelected] = useState<string | null>(null)
  const [pending, setPending] = useState(false)
  const [status, setStatus] = useState('')
  const [editing, setEditing] = useState<Account | 'new' | null>(null)
  const failed = source.error !== null
  const busy = pending || list === undefined

  // Successful changes come back as accountsChanged.
  const run = (message: string, work: Promise<unknown>, describe: (error: string) => string = error => error) => {
    setPending(true)
    setStatus(message)
    work.then(() => {
      setPending(false)
      setStatus('')
    }, (reason: unknown) => {
      setPending(false)
      setStatus(describe(messageOf(reason)))
      source.refresh()
    })
  }

  const accounts = list?.accounts ?? []
  const activeId = list?.activeAccountId
  const active = accounts.find(account => account.id === activeId)
  const chosen = accounts.find(account => account.id === selected) ?? active ?? accounts[0]
  const chosenActive = chosen !== undefined && chosen.id === activeId
  const providerOf = (account: Account) => list?.presets.find(preset => preset.id === account.providerId)?.label ?? t.customProvider
  const notice = status !== '' ? status
    : list === undefined ? t.loadingAccounts
      : failed ? t.coreUnavailable(source.error ?? '')
        : accounts.length === 0 ? t.noAccounts
          : active === undefined ? t.noActiveAccount
            : ''

  const save = (account: Account, apiKey: string) => {
    setEditing(null)
    const first = activeId === undefined
    // The first account becomes the default.
    const work = bridge.saveAccount(account, apiKey).then((result) => {
      const id = (result as { account?: { id?: string } } | null)?.account?.id
      return first && id ? bridge.activateAccount(id, '') : undefined
    })
    run(t.savingAccount, work, t.saveFailed)
  }

  return (
    <section className={css.section}>
      <h3 className={css.heading}>{t.accountsSection}</h3>
      <div className={css.accounts} role="listbox" aria-label={t.accountsSection} aria-busy={busy}>
        {accounts.map(account => (
          <div
            key={account.id}
            role="option"
            tabIndex={0}
            aria-selected={account === chosen}
            className={clsx(css.account, account === chosen && css.chosen)}
            title={account.baseUrl}
            onClick={() => { setSelected(account.id ?? null) }}
            onDoubleClick={() => { if (!busy) setEditing(account) }}
            onKeyDown={(event) => {
              if (event.key === 'Enter' && !busy) setEditing(account)
            }}
          >
            <span className={clsx(css.accountName, account.id === activeId && css.active)}>
              {account.id === activeId ? '✓ ' : ''}{account.name}
            </span>
            <span className={css.accountMeta}>{providerOf(account)} · {account.model}</span>
          </div>
        ))}
      </div>
      <div className={css.buttons}>
        <Button variant="outline" disabled={busy || failed} onClick={() => { setEditing('new') }}>{t.accountAdd}</Button>
        <Button variant="outline" disabled={busy || chosen === undefined} onClick={() => { if (chosen) setEditing(chosen) }}>{t.accountEdit}</Button>
        {/* AI Core refuses to delete the account in use. */}
        <Button
          variant="outline"
          disabled={busy || chosen === undefined || chosenActive}
          onClick={() => { if (chosen?.id) run(t.deletingAccount, bridge.deleteAccount(chosen.id), t.deleteFailed) }}
        >
          {t.delete}
        </Button>
        <span className={css.spacer} />
        <Button
          variant="primary"
          disabled={busy || chosen === undefined || chosenActive}
          onClick={() => { if (chosen?.id) run(t.switchingAccount, bridge.activateAccount(chosen.id, '')) }}
        >
          {t.accountActivate}
        </Button>
      </div>
      <div className={css.modelRow}>
        <span>{t.currentModel}</span>
        <select
          value={active?.model ?? ''}
          disabled={busy || active === undefined}
          aria-label={t.currentModel}
          onChange={(event) => { if (active?.id) run(t.switchingModel, bridge.activateAccount(active.id, event.target.value)) }}
        >
          {idsOf(active?.models).map(id => <option key={id} value={id}>{id}</option>)}
        </select>
      </div>
      {notice !== '' && <p className={css.hint} role="status">{notice}</p>}
      {editing !== null && list !== undefined && (
        <AccountEditor
          presets={list.presets}
          original={editing === 'new' ? undefined : editing}
          onCancel={() => { setEditing(null) }}
          onSave={save}
        />
      )}
    </section>
  )
}

export function SettingsDialog({ bridge, accounts, onClose }: { bridge: Bridge; accounts: AccountsSource; onClose: () => void }) {
  const [mcpServers, setMcpServers] = useState<string | null>(null)
  const [systemPrompt, setSystemPrompt] = useState('')
  const [problem, setProblem] = useState('')
  useEffect(() => {
    void bridge.settings().then((settings) => {
      setMcpServers(settings.mcpServers)
      setSystemPrompt(settings.systemPrompt)
    })
  }, [bridge])
  const save = () => {
    if (mcpServers === null) return
    void bridge.saveSettings({ mcpServers, systemPrompt }).then((error) => {
      if (error === '') onClose()
      else setProblem(error)
    })
  }
  return (
    <Modal
      open
      onClose={onClose}
      closeLabel={t.close}
      title={t.settingsTitle}
      className={css.dialog}
      contentClassName={css.content}
      footer={(
        <>
          <Button variant="outline" onClick={onClose}>{t.cancel}</Button>
          <Button variant="primary" disabled={mcpServers === null} onClick={save}>{t.save}</Button>
        </>
      )}
    >
      <Accounts bridge={bridge} source={accounts} />
      <section className={css.section}>
        <Field label={t.mcpServers}>
          {id => (
            <textarea
              id={id}
              className={css.code}
              rows={4}
              spellCheck={false}
              value={mcpServers ?? ''}
              disabled={mcpServers === null}
              aria-invalid={problem !== ''}
              placeholder={MCP_EXAMPLE}
              onChange={(event) => {
                setMcpServers(event.target.value)
                setProblem('')
              }}
            />
          )}
        </Field>
        {problem !== '' && <p className={css.error} role="alert">{problem}</p>}
        <Field label={t.systemPrompt}>
          {id => (
            <textarea
              id={id}
              rows={3}
              value={systemPrompt}
              disabled={mcpServers === null}
              onChange={(event) => { setSystemPrompt(event.target.value) }}
            />
          )}
        </Field>
        <p className={css.hint}>{t.settingsHint}</p>
      </section>
    </Modal>
  )
}
