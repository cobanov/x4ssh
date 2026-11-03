# Rehearsal: x4ssh hero

Direction (unslop):
- Product: firmware that turns a pocket e-ink reader into an SSH terminal typed on with a Bluetooth keyboard.
- Character: slow, exact, a little funny.
- World: the design kit: two inks reversed, dither for tone, stepped corners, the status bar as a band.
- Moments: the panel in the hero, playing frames the firmware's renderer drew, and the snail sweeping its dish beside the headline.
- Signature: the block cursor after the headline.

Gold: `dark-hero` and `phone-touch`, through deckterm's hero (site/hero there): one device across the column, an even
band above and below, keystrokes under it. Changed: no kit Phone, the X4's body is drawn; no scene markup, the panel is
an image of the real renderer's output, swapped once per refresh.

Beats: asleep → POWER, a full refresh (black, white, black) → the snail sweeps its dish, wifi connects → `ssh
cobanov@nuc` in bursts of what one refresh catches → ↵, the art comes down and the login scrolls the splash away →
`uptime -p` ↵ → `docker ps --format {{.Names}}` ↵ → hold → POWER, a full refresh, asleep, where the loop began. Loop
about 20 s, over the 10 to 16 s the skill wants: a fast refresh is 0.4 s and shortening that would be a lie about the
product. The full refresh is shortened from 3.5 s to 0.7 s.

Wide (1000 × 680): body whole across the column, panel at 1:1 from a 1040px viewport up, 40 above, 56 under for keys.
Compact (360 × 461, below 560px): body at two thirds, off both sides of the screen, grown by `fill` on wider phones;
the stage pans from the snail (from 245 across the panel, so the status bar is not sliced) to the left margin when
typing starts, and back at sleep. The session's lines stay inside the crop's 52 columns.

Captured: full loop at 1280 and 390 at 1.5 s; the reduced-motion poster at 1280; page at 1440, 800, 390 and 360 (no
horizontal scroll at any, no console or network errors).

Film lines left:
- `uneven gutters` on `.x4-panel` in `.x4-body`: the X4's panel really sits 40 from one end and 160 from the other, the
  chin with its buttons. Same case as deckterm's `uneven padding`. `--gate` ends failed on this line alone.
- `inconclusive`: five sampling gaps over 26ms in headless Chrome; motion here is image swaps and one pan.
- `scale: 1.08` at 390: `fill` growing the compact design to the phone's width, on purpose.

Fixed on the way: the panel's ink status bar melted into the ink body (2px paper line around the panel); the crop cut IPs and the last-login line (lines shortened); status text sliced at both edges on the
splash (pan moved).

Assumptions: the chin is on the right when held sideways, from CrossPoint's portrait rotation, not checked against the
device. Body proportions from a product picture, drawn as a line around a solid form and the published 114 × 69 mm. The login text, hosts and containers are
made up in the shape of deckterm's; the network name `homelab` is not a real one.

Not checked: Safari and Firefox, a real phone, the deployed site. No fresh-eye reviewer was run.
