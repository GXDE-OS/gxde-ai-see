import { useEffect, useState } from 'react'
import type { Bridge } from './bridge'
import { connectBridge } from './bridge'

/** The native bridge once connected; null before that and outside the host. */
export function useBridge(): Bridge | null {
  const [bridge, setBridge] = useState<Bridge | null>(null)
  useEffect(() => {
    let alive = true
    void connectBridge().then((connected) => {
      if (alive) setBridge(connected)
    })
    return () => {
      alive = false
    }
  }, [])
  return bridge
}
