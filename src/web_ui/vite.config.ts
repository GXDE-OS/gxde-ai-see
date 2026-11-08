import react from '@vitejs/plugin-react'
import { defineConfig } from 'vite'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  // Use relative asset paths so the build output can be loaded from file:// or qrc:// by the Qt host
  base: './',
  // LLMs write TeX as \(…\) and \[…\]; this fork of the math syntax reads both
  resolve: {
    alias: { 'micromark-extension-math': 'micromark-extension-llm-math' },
  },
})
