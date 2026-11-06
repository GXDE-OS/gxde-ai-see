import { useEffect } from 'react'
import { useChat } from './chat/useChat'
import { ConversationPanel } from './components/ConversationPanel'
import { Sidebar } from './components/Sidebar'
import frame from './vendor/dsh/layout/AppFrame.module.css'

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

export function App() {
  useColorScheme()
  const chat = useChat()

  return (
    <div className={frame.frame} style={{ gridTemplateColumns: `${SIDEBAR_WIDTH}px minmax(0, 1fr) 0px` }}>
      <div className={frame.sidebarCol}>
        <Sidebar
          sessions={chat.conversations}
          currentId={chat.current?.id ?? null}
          onNew={() => {
            chat.open(null)
          }}
          onOpen={chat.open}
          onRename={chat.rename}
          onDelete={chat.remove}
        />
      </div>
      <div className={frame.centerCol}>
        <ConversationPanel conversation={chat.current} running={chat.running} onSend={chat.send} onStop={chat.stop} />
      </div>
      <div className={frame.rightbarCol} />
    </div>
  )
}
