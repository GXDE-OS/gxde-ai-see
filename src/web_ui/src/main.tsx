import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import './vendor/dsh/theme/design-platform.css'
import './vendor/dsh/theme/base.css'
import './vendor/dsh/theme/gradient-shadow-text.css'
import './vendor/dsh/theme/focus.css'
import './vendor/dsh/theme/scrollbar.css'
import './vendor/dsh/primitives/input-modality.ts'
import './index.css'
import './dtk-menu.css'
import { App } from './App.tsx'

if (import.meta.env.DEV && window.gxdeBridge === undefined) {
  const { installDevBridge } = await import('./ipc/devBridge')
  installDevBridge()
}

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <App />
  </StrictMode>,
)
