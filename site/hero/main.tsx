import { createRoot } from 'react-dom/client'
import { X4Hero } from './X4Hero'

const host = document.getElementById('hero-demo')
if (host) createRoot(host).render(<X4Hero />)
