import { useCallback, useEffect, useState } from 'react'
import clsx from 'clsx'
import { useDrop } from './chat/drop'
import { t } from './chat/strings'
import { useAccounts, useModes } from './chat/useBackend'
import { useChat } from './chat/useChat'
import { ModelSelect, ModeSelect } from './components/ComposerTools'
import { ConversationPanel } from './components/ConversationPanel'
import { SettingsDialog } from './components/SettingsDialog'
import { Sidebar } from './components/Sidebar'
import { useBridge } from './ipc/useBridge'
import frame from './vendor/dsh/layout/AppFrame.module.css'
import local from './App.module.css'

const SIDEBAR_WIDTH = 280

function useColorScheme() {
  useEffect(() => {
    const query = window.matchMedia('(prefers-color-scheme: dark)')
    const apply = () => {
      document.body.toggleAttribute('data-ds-dark-theme', query.matches)
    }

    apply()
    query.addEventListener('change', apply)
    return () => {
      query.removeEventListener('change', apply)
    }
  }, [])
}

const fileName = (path: string) => path.split('/').filter(Boolean).at(-1) ?? path

export function App() {
  useColorScheme()
  const bridge = useBridge()
  const chat = useChat(bridge)
  const accounts = useAccounts(bridge)
  const modes = useModes(bridge)
  const [draft, setDraft] = useState('')
  // Title for the conversation an ask from the panel starts.
  const [askTitle, setAskTitle] = useState<string | undefined>(undefined)
  const [settingsOpen, setSettingsOpen] = useState(false)
  const [notice, setNotice] = useState<string | undefined>(undefined)
  const [focusRequest, setFocusRequest] = useState(0)

  const { open: openChat, send: sendChat } = chat
  const open = useCallback((id: string | null) => {
    setAskTitle(undefined)
    openChat(id)
  }, [openChat])

  const insert = useCallback((text: string) => {
    setDraft(current => current === '' || current.endsWith('\n') ? current + text : `${current}\n${text}`)
    setFocusRequest(n => n + 1)
  }, [])
  useDrop(bridge, open, insert)

  useEffect(() => {
    if (bridge === null) return
    const off = bridge.on('askRequested', (text, files) => {
      openChat(null)
      setDraft([text, ...files].filter(part => part !== '').join('\n'))
      const title = text === '' ? fileName(files[0] ?? '') : text.replace(/\s+/g, ' ').trim().slice(0, 24)
      setAskTitle(title === '' ? undefined : title)
      setFocusRequest(n => n + 1)
    })
    // Asks queued before the page listened arrive now.
    bridge.ready()
    return off
  }, [bridge, openChat])

  const { refresh: refreshAccounts } = accounts
  useEffect(() => bridge?.on('coreRestarted', () => {
    // Asking for the accounts starts AI Core again.
    refreshAccounts()
    setNotice(t.coreRestarted)
  }), [bridge, refreshAccounts])

  const send = sendChat && ((text: string) => {
    setNotice(undefined)
    sendChat(text, chat.current === undefined ? askTitle : undefined)
    setAskTitle(undefined)
  })

  const approval = chat.approvals.find(item => item.conversationId === chat.current?.id)

  return (
    <div className={clsx(frame.frame, local.frame)} style={{ gridTemplateColumns: `${SIDEBAR_WIDTH}px minmax(0, 1fr) 0px` }}>
      <div className={clsx(frame.sidebarCol, local.sidebarCol)}>
        <Sidebar
          sessions={chat.conversations}
          currentId={chat.current?.id ?? null}
          onNew={() => {
            open(null)
          }}
          onOpen={open}
          onRename={chat.rename}
          onDelete={chat.remove}
          runningId={chat.runningId}
          waitingIds={chat.approvals.map(item => item.conversationId)}
          onSettings={bridge === null ? undefined : () => {
            setSettingsOpen(true)
          }}
        />
      </div>
      <div className={clsx(frame.centerCol, local.centerCol)}>
        <ConversationPanel
          conversation={chat.current}
          running={chat.running}
          busy={chat.runningId !== null && !chat.running}
          activityAt={chat.activityAt}
          approval={approval}
          draft={draft}
          onDraftChange={setDraft}
          tools={(
            <>
              <ModeSelect {...modes} />
              <ModelSelect
                accounts={accounts}
                provider={chat.provider}
                onSelect={chat.setProvider}
                onManage={() => {
                  setSettingsOpen(true)
                }}
              />
            </>
          )}
          notice={notice}
          focusRequest={focusRequest}
          onSend={send}
          onStop={chat.stop}
          onAnswer={chat.answer}
          onRetry={chat.retry}
          onEdit={chat.edit}
          onFork={chat.fork}
        />
      </div>
      <div className={frame.rightbarCol} />
      {settingsOpen && bridge !== null && (
        <SettingsDialog
          bridge={bridge}
          accounts={accounts}
          onClose={() => {
            setSettingsOpen(false)
          }}
        />
      )}
    </div>
  )
}
