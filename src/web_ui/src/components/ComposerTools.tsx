/*
 * Heavily referenced DeepSeek Harness ui-permission-presets PermissionSelect
 * (MIT, see src/vendor/dsh/README.md).
 */
import { useState } from 'react'
import type { ReactNode } from 'react'
import clsx from 'clsx'
import acceptEditsIcon from '../assets/icons/accept_edits_on.svg'
import manualApprovalIcon from '../assets/icons/manual_approval.svg'
import planModeIcon from '../assets/icons/plan_mode.svg'
import yoloIcon from '../assets/icons/yolo.svg'
import { t } from '../chat/strings'
import type { Accounts, Modes } from '../chat/useBackend'
import type { Provider } from '../ipc/bridge'
import { IconCheckOutlineRegular, IconChevronDownOutlineRegular } from '../vendor/dsh/primitives/icons/index.tsx'
import { Menu } from '../vendor/dsh/primitives/Menu.tsx'
import type { MenuEntry } from '../vendor/dsh/primitives/Menu.tsx'
import { RiskConfirmation } from '../vendor/dsh/primitives/RiskConfirmation.tsx'
import css from '../vendor/dsh/permission/PermissionSelect.module.css'
import local from './ComposerTools.module.css'
import { MaskIcon } from './MaskIcon'

const YOLO = 'yolo'

const MODE_ICONS: Record<string, string> = {
  plan: planModeIcon,
  autoEdit: acceptEditsIcon,
  [YOLO]: yoloIcon,
}

function modeGlyph(id: string): ReactNode {
  return <MaskIcon src={MODE_ICONS[id] ?? manualApprovalIcon} size={14} />
}

function Trigger({ icon, label, aria, title, open, onClick }: {
  icon?: ReactNode
  label: string
  aria: string
  title?: string | undefined
  open: boolean
  onClick: () => void
}) {
  return (
    <button type="button" className={css.trigger} aria-label={aria} title={title} onClick={onClick}>
      {icon !== undefined && <span className={css.triggerIcon} aria-hidden>{icon}</span>}
      <span className={css.triggerLabel}>{label}</span>
      <span className={clsx(css.chevron, open && css.chevronOpen)} aria-hidden>
        <IconChevronDownOutlineRegular />
      </span>
    </button>
  )
}

export function ModeSelect({ modes, mode, setMode }: Modes) {
  const [open, setOpen] = useState(false)
  const [confirming, setConfirming] = useState(false)
  const [acknowledged, setAcknowledged] = useState(false)
  if (modes.length === 0) return null
  const current = modes.find(option => option.id === mode)
  const items: MenuEntry[] = modes.map(option => ({ id: option.id, label: option.label, icon: modeGlyph(option.id) }))
  const choose = (id: string) => {
    setOpen(false)
    if (id === mode) return
    if (id !== YOLO) {
      setMode(id)
      return
    }
    setAcknowledged(false)
    setConfirming(true)
  }
  return (
    <>
      <Menu
        open={open}
        items={items}
        selectedId={mode}
        onSelect={choose}
        onClose={() => {
          setOpen(false)
        }}
        side="top"
        portal
        anchor={(
          <Trigger
            icon={modeGlyph(mode)}
            label={current?.label ?? mode}
            aria={t.modeAria(current?.label ?? mode)}
            title={t.modeHints[mode]}
            open={open}
            onClick={() => {
              setOpen(!open)
            }}
          />
        )}
      />
      {confirming && (
        <RiskConfirmation
          open
          title={t.yoloTitle}
          description={t.yoloDescription}
          acknowledgeLabel={t.yoloAcknowledge}
          cancelLabel={t.cancel}
          closeLabel={t.close}
          confirmLabel={modes.find(option => option.id === YOLO)?.label ?? YOLO}
          acknowledged={acknowledged}
          onAcknowledgedChange={setAcknowledged}
          onCancel={() => {
            setConfirming(false)
          }}
          onConfirm={() => {
            setConfirming(false)
            setMode(YOLO)
          }}
        />
      )}
    </>
  )
}

const MANAGE = '\0manage'
const SEPARATOR = '\u0001'

export function ModelSelect({ accounts, provider, onSelect, onManage }: {
  accounts: Accounts
  /** The conversation's pick; unset follows AI Core's default account. */
  provider: Provider | undefined
  onSelect: (provider: Provider) => void
  onManage: () => void
}) {
  const [open, setOpen] = useState(false)
  const { list } = accounts
  if (list === undefined) return null
  const account = list.accounts.find(item => item.id === (provider?.accountId ?? list.activeAccountId))
  const model = provider?.model || account?.model || ''
  const label = account === undefined
    ? (provider === undefined ? t.noAccount : t.accountGone)
    : `${account.name} · ${model}`
  const items: MenuEntry[] = [
    ...list.accounts.filter(item => item.id !== undefined).map(item => ({
      id: item.id!,
      label: item.id === list.activeAccountId ? t.defaultAccount(item.name) : item.name,
      disabled: item.models.length === 0,
      submenu: item.models.map(info => ({
        id: `${item.id}${SEPARATOR}${info.id}`,
        label: info.id,
        icon: item === account && info.id === model ? <IconCheckOutlineRegular /> : <span className={local.checkSpace} />,
      })),
    })),
    ...(list.accounts.length === 0 ? [] : [{ type: 'separator' as const, id: 'sep' }]),
    { id: MANAGE, label: t.manageAccounts },
  ]
  return (
    <Menu
      open={open}
      items={items}
      selectedId={account?.id}
      onSelect={(id) => {
        setOpen(false)
        if (id === MANAGE) {
          onManage()
          return
        }
        const [accountId = '', modelId = ''] = id.split(SEPARATOR)
        if (modelId !== '') onSelect({ accountId, model: modelId })
      }}
      onClose={() => {
        setOpen(false)
      }}
      side="top"
      portal
      anchor={(
        <Trigger
          label={label}
          aria={t.modelAria(label)}
          title={accounts.error ?? undefined}
          open={open}
          onClick={() => {
            if (list.accounts.length === 0) {
              onManage()
              return
            }
            // The CLI may have changed them since.
            if (!open) accounts.refresh()
            setOpen(!open)
          }}
        />
      )}
    />
  )
}
