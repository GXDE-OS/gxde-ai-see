import { useEffect } from 'react'
import type { Bridge } from '../ipc/bridge'
import { t } from './strings'

/** A dropped URL as a line for the model: local files by path. */
export function urlLine(url: string): string {
  if (!url.startsWith('file:')) return url
  try {
    return t.droppedFile(decodeURIComponent(new URL(url).pathname))
  }
  catch {
    return url
  }
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

async function dropText(bridge: Bridge, data: DataTransfer): Promise<string> {
  const urls = data.getData('text/uri-list').split(/\r?\n/).filter(line => line !== '' && !line.startsWith('#'))
  if (urls.length > 0) return urls.map(urlLine).join('\n')
  const image = [...data.files].find(file => file.type.startsWith('image/'))
  if (image !== undefined) {
    const path = await bridge.saveImage(await readBase64(image))
    return path === '' ? '' : t.droppedImage(path)
  }
  return data.getData('text/plain')
}

const accepts = (data: DataTransfer | null) =>
  data !== null && data.types.some(type => type === 'Files' || type === 'text/uri-list' || type === 'text/plain')

/**
 * Turn anything dropped on the page into composer text. A drop on a
 * conversation opens it; one on the list's empty space starts a new one.
 */
export function useDrop(bridge: Bridge | null, open: (id: string | null) => void, insert: (text: string) => void) {
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
      void dropText(bridge, data!).then((text) => {
        if (text.trim() !== '') insert(text)
      })
    }
    window.addEventListener('dragover', over, true)
    window.addEventListener('drop', drop, true)
    const off = bridge.on('urlsDropped', (urls, x, y) => {
      target(x, y)
      insert(urls.map(urlLine).join('\n'))
    })
    return () => {
      window.removeEventListener('dragover', over, true)
      window.removeEventListener('drop', drop, true)
      off()
    }
  }, [bridge, insert, open])
}
