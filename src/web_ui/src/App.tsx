import { useEffect } from 'react'

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
  return null
}
