// The hero, built into public/hero as one script and one stylesheet the page links by name. Wrangler runs this
// before every deploy. The frames stay in public/film, written by ../film.sh.
import { defineConfig } from 'vite'

export default defineConfig({
  esbuild: { jsx: 'automatic' },
  build: {
    outDir: '../public/hero',
    emptyOutDir: true,
    rollupOptions: {
      input: 'main.tsx',
      output: { entryFileNames: 'hero.js', assetFileNames: 'hero[extname]' },
    },
  },
})
