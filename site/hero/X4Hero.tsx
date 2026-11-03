import { useEffect } from 'react'
// From the kit's own files, not its index, so the page bundles only these parts and their styles
import { Stage } from './kit/stage'
import type { Act } from './kit/act'
import film from './film.json'
import './x4.css'

// The site hero: the X4 held sideways, playing frames the firmware's own renderer drew (../film.cpp).
// Beats: asleep with the power off; power; the snail sweeps its dish until wifi answers; `ssh cobanov@nuc`;
// the login scrolls the splash away; two commands; power again, and it is back asleep, where the loop began.
//
// One frame is one panel refresh. A fast refresh is a plain swap, as it looks on the panel; a full one flashes
// black and white first. Its 3.5 s on the real panel is shortened here to keep the loop moving.
//
// The body is drawn from the X4's real shape, in the kit's two inks: a narrow bezel, the wide chin with its two long buttons.
// Sideways the chin is on the right, as the panel's own origin puts it. Panel at 1:1 from 1040px up, so its
// pixels stay pixels.

type S = { frame: number; flash: null | 'black' | 'white'; pan: 'art' | 'text' }

const OUTPUT = film.findIndex((f) => f.mark === 'output')
const initial: S = { frame: 0, flash: null, pan: 'art' }
const poster: S = { frame: OUTPUT, flash: null, pan: 'text' }

const src = (i: number) => `/film/${String(i).padStart(3, '0')}.png`

async function script(a: Act<S>) {
  for (let i = 0; i < film.length; i++) {
    const f = film[i]
    if (i > 0) {
      if (f.key) await a.key(f.key)
      if (f.mark) a.mark(f.mark)
      if (f.mark === 'typing') a.put({ pan: 'text' })
      if (f.mark === 'sleep') a.put({ pan: 'art' })
      if (f.kind === 'full') {
        a.put({ flash: 'black' })
        await a.wait(240)
        a.put({ flash: 'white' })
        await a.wait(240)
        a.put({ flash: 'black' })
        await a.wait(240)
        a.put({ flash: null, frame: i })
      } else {
        a.put({ frame: i })
      }
    }
    await a.wait(f.hold)
  }
  // The last frame is the first one again
  await a.snap({ frame: 0 })
}

const PANEL = { w: 800, h: 480 }
const BEZEL = { left: 40, right: 160, y: 52 }
const BODY = { w: PANEL.w + BEZEL.left + BEZEL.right, h: PANEL.h + BEZEL.y * 2 }
const TOP = 40
const BAND = 56 // under the body, where the keystrokes show

// In a phone's column the whole panel would be too small to read, so it is drawn at two thirds and the stage pans:
// to the snail on the splash, to the left margin once text is what matters. The session's lines are kept to 52 columns, the crop's width. Two thirds is 2 device pixels per panel
// pixel on a 3x phone.
const SCALE = 2 / 3
const COMPACT = { w: 360, h: Math.round(16 + BODY.h * SCALE + 56) }
const PAN = {
  art: Math.round(-(BEZEL.left + 245) * SCALE), // from just past the left of the status bar, so nothing in it is sliced
  text: Math.round(-BEZEL.left * SCALE),
}

// A rectangle with pixel steps in its corners, as the snail's outline would draw it
function stepped(w: number, h: number, s = 8, n = 3) {
  const c: number[][] = []
  for (let i = n; i >= 1; i--) c.push([i * s, (n - i) * s], [i * s, (n - i + 1) * s])
  c.push([0, n * s])
  const back = [...c].reverse()
  const points = [
    ...c,
    ...back.map(([x, y]) => [x, h - y]),
    ...c.map(([x, y]) => [w - x, h - y]),
    ...back.map(([x, y]) => [w - x, y]),
  ]
  return `polygon(${points.map(([x, y]) => `${x}px ${y}px`).join(',')})`
}

const LINE = 4 // the body's outline

export function X4Hero() {
  // Every frame is in the cache before its refresh, so a swap never shows a gap
  useEffect(() => {
    film.forEach((_, i) => {
      new Image().src = src(i)
    })
  }, [])
  return (
    <Stage<S>
      width={BODY.w}
      height={TOP + BODY.h + BAND}
      compact={{ below: 560, width: COMPACT.w, height: COMPACT.h }}
      fill
      initial={initial}
      poster={poster}
      script={script}
      className="x4"
    >
      {(s, _rect, layout) =>
        layout === 'compact' ? (
          <div className="x4-crop">
            <Device s={s} style={{ transform: `translate(${PAN[s.pan]}px, 16px) scale(${SCALE})` }} />
          </div>
        ) : (
          <Device s={s} style={{ transform: `translateY(${TOP}px)` }} />
        )
      }
    </Stage>
  )
}

function Device({ s, style }: { s: S; style: React.CSSProperties }) {
  return (
    <div className="x4-body" style={{ ...style, width: BODY.w, height: BODY.h, clipPath: stepped(BODY.w, BODY.h) }}>
      <i
        className="x4-fill"
        style={{ inset: LINE, clipPath: stepped(BODY.w - LINE * 2, BODY.h - LINE * 2) }}
      />
      <div className="x4-panel" style={{ left: BEZEL.left, top: BEZEL.y, width: PANEL.w, height: PANEL.h }}>
        <img src={src(s.frame)} width={PANEL.w} height={PANEL.h} alt="" data-snap />
        {s.flash && <i className="x4-flash" data-flash={s.flash} data-snap />}
      </div>
      <span className="x4-button" style={{ left: BEZEL.left + PANEL.w + 72, top: 96 }} />
      <span className="x4-button" style={{ left: BEZEL.left + PANEL.w + 72, top: BODY.h / 2 + 8 }} />
    </div>
  )
}
