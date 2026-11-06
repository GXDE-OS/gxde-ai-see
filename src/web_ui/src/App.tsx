import { useEffect, useState } from 'react'
import { ConversationPanel } from './components/ConversationPanel'
import { Sidebar } from './components/Sidebar'
import type { Conversation } from './ipc/bridge'
import { connectBridge } from './ipc/bridge'
import frame from './vendor/dsh/layout/AppFrame.module.css'

const SIDEBAR_WIDTH = 280

function useConversations(): Conversation[] {
  const [conversations, setConversations] = useState<Conversation[]>([])
  useEffect(() => {
    let alive = true
    void connectBridge()
      .then(bridge => bridge?.loadConversations())
      .then((store) => {
        if (!alive || store === undefined) return
        const list = [...(store.conversations ?? [])].sort((a, b) => b.updatedAt - a.updatedAt)
        setConversations(list)
      })
    return () => {
      alive = false
    }
  }, [])
  return conversations
}

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

export function App() {
  useColorScheme()
  const conversations = useConversations()
  const [currentId, setCurrentId] = useState<string | null>(null)
  const current = conversations.find(conversation => conversation.id === currentId)

  return (
    <div className={frame.frame} style={{ gridTemplateColumns: `${SIDEBAR_WIDTH}px minmax(0, 1fr) 0px` }}>
      <div className={frame.sidebarCol}>
        <Sidebar
          sessions={conversations}
          currentId={currentId}
          onNew={() => {
            setCurrentId(null)
          }}
          onOpen={setCurrentId}
        />
      </div>
      <div className={frame.centerCol}>
        <ConversationPanel conversation={current} />
      </div>
      <div className={frame.rightbarCol} />
    </div>
  )
}
