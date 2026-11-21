import { useEffect } from 'react'
import type { Attachment, Bridge } from '../ipc/bridge'

const marker = (path: string) => {
  const name = path.split('/').filter(Boolean).at(-1) ?? path
  return { path, name, image: false }
}

function readBase64(file: File): Promise<string> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader()
    reader.onload = () => {
      const data = String(reader.result)
      resolve(data.slice(data.indexOf(',') + 1))
    }
    reader.onerror = () => {
      reject(reader.error ?? new Error('read failed'))
    }
    reader.readAsDataURL(file)
  })
}

interface DroppedContent {
  text: string
  files: Attachment[]
}

/** Drop payload 拆成「输入框文字」和「附件」两部分。 */
async function dropContent(bridge: Bridge, data: DataTransfer): Promise<DroppedContent> {
  const urls = data.getData('text/uri-list').split(/\r?\n/).filter(line => line !== '' && !line.startsWith('#'))
  const files: Attachment[] = []
  const texts: string[] = []
  for (const url of urls) {
    if (url.startsWith('file:')) {
      try {
        files.push(marker(decodeURIComponent(new URL(url).pathname)))
      }
      catch {
        texts.push(url)
      }
    }
    else {
      texts.push(url)
    }
  }
  if (urls.length > 0) {
    return { text: texts.join('\n'), files }
  }
  const image = [...data.files].find(file => file.type.startsWith('image/'))
  if (image !== undefined) {
    const path = await bridge.saveImage(await readBase64(image))
    return { text: '', files: path === '' ? [] : [marker(path)] }
  }
  return { text: data.getData('text/plain'), files: [] }
}

const accepts = (data: DataTransfer | null) =>
  data !== null && data.types.some(type => type === 'Files' || type === 'text/uri-list' || type === 'text/plain')

/**
 * Turn anything dropped on the page into composer text / attachments. A drop
 * on a conversation opens it; one on the list's empty space starts a new one.
 */
export function useDrop(
  bridge: Bridge | null,
  open: (id: string | null) => void,
  insertText: (text: string) => void,
  addAttachments: (files: Attachment[]) => void,
) {
  useEffect(() => {
    if (bridge === null) return
    const target = (x: number, y: number) => {
      const element = document.elementFromPoint(x, y)
      const row = element?.closest<HTMLElement>('[data-session-id]')
      if (row) open(row.dataset.sessionId ?? null)
      else if (element?.closest('[data-drop-new]')) open(null)
    }
    const over = (event: DragEvent) => {
      if (!accepts(event.dataTransfer)) return
      event.preventDefault()
      event.dataTransfer!.dropEffect = 'copy'
    }
    const drop = (event: DragEvent) => {
      const data = event.dataTransfer
      if (!accepts(data)) return
      // Also keeps a textarea from taking the text and the page from navigating.
      event.preventDefault()
      target(event.clientX, event.clientY)
      void dropContent(bridge, data!).then(({ text, files }) => {
        if (text.trim() !== '') insertText(text)
        addAttachments(files)
      })
    }
    window.addEventListener('dragover', over, true)
    window.addEventListener('drop', drop, true)
    const off = bridge.on('urlsDropped', (urls, x, y) => {
      target(x, y)
      const files: Attachment[] = []
      const texts: string[] = []
      for (const url of urls) {
        if (url.startsWith('file:')) {
          try {
            files.push(marker(decodeURIComponent(new URL(url).pathname)))
          }
          catch {
            texts.push(url)
          }
        }
        else {
          texts.push(url)
        }
      }
      if (texts.length > 0) insertText(texts.join('\n'))
      addAttachments(files)
    })
    return () => {
      window.removeEventListener('dragover', over, true)
      window.removeEventListener('drop', drop, true)
      off()
    }
  }, [bridge, insertText, addAttachments, open])
}
