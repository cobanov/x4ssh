# Builds every x4ssh asset from the sources here: assets/ for the kit, include/mascot.h for the firmware.
#   uvx --with pillow --with numpy python design/mascot/build.py
import os, subprocess, tempfile, numpy as np
from PIL import Image
from marks import MARK16, MARK24, grid, wordmark
from snail3d import pose

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'assets')
HEADER = os.path.join(HERE, '..', '..', 'include', 'mascot.h')
INK, PAPER = '#222222', '#E2E0D8'
SCALE = 3                     # device pixels per art pixel on the X4
SCREEN_W, FIRST_ROW, ROW_H, OFFSET_Y = 800, 1, 20, 12

def rgb(hexcolor): return np.array([int(hexcolor[i:i + 2], 16) for i in (1, 3, 5)], np.uint8)

def png(a, path, k=1, fg=INK, bg=PAPER):
    big = np.kron(a, np.ones((k, k), bool))
    Image.fromarray(np.where(big[..., None], rgb(fg), rgb(bg)).astype(np.uint8)).save(path)

def svg(a, path, fg=INK, bg=None, title='x4ssh'):
    """Pixels as one path of row runs, so the art stays crisp at any integer scale."""
    h, w = a.shape
    d = []
    for y in range(h):
        x = 0
        while x < w:
            if not a[y, x]:
                x += 1
                continue
            x0 = x
            while x < w and a[y, x]: x += 1
            d.append(f'M{x0} {y}h{x - x0}v1h-{x - x0}z')
    back = f'<rect width="{w}" height="{h}" fill="{bg}"/>' if bg else ''
    with open(path, 'w') as f:
        f.write(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" shape-rendering="crispEdges">'
                f'<title>{title}</title>{back}<path fill="{fg}" d="{"".join(d)}"/></svg>\n')

def crop(ims):
    """One box for every pose, so the snail stays put when the pose changes."""
    ys, xs = np.where(np.logical_or.reduce(ims))
    return ys.min(), ys.max() + 1, xs.min(), xs.max() + 1

def pack(a):
    h, w = a.shape
    return np.packbits(np.pad(a, ((0, 0), (0, -w % 8))), axis=1).reshape(-1)

def carray(name, a):
    b = pack(a)
    rows = [', '.join(f'0x{v:02x}' for v in b[i:i + 20]) for i in range(0, len(b), 20)]
    return f'inline constexpr uint8_t {name}[] = {{\n  ' + ',\n  '.join(rows) + '\n};\n'

def lockups():
    m, w = grid(MARK24), wordmark()
    w2 = np.kron(w, np.ones((2, 2), bool))
    gap = 6
    horizontal = np.zeros((m.shape[0], m.shape[1] + gap + w2.shape[1]), bool)
    horizontal[:, :m.shape[1]] = m
    horizontal[-w2.shape[0]:, m.shape[1] + gap:] = w2
    m2 = np.kron(m, np.ones((2, 2), bool))
    stacked = np.zeros((m2.shape[0] + 8 + w2.shape[0], w2.shape[1]), bool)
    x = (w2.shape[1] - m2.shape[1]) // 2
    stacked[:m2.shape[0], x:x + m2.shape[1]] = m2
    stacked[-w2.shape[0]:] = w2
    return dict(horizontal=horizontal, stacked=stacked, wordmark=w)

def tile(a, k, size, fg, bg):
    """The art at an exact integer scale, centred on a square: resampling pixel art blurs it."""
    im = Image.new('RGBA', (size, size), bg)
    big = np.kron(a, np.ones((k, k), bool))
    h, w = big.shape
    im.paste(fg, ((size - w) // 2, (size - h) // 2), Image.fromarray((big * 255).astype(np.uint8)))
    return im

def web_icons():
    ink, paper, clear = rgb(INK), rgb(PAPER), (0, 0, 0, 0)
    ink, paper = tuple(int(v) for v in ink) + (255,), tuple(int(v) for v in paper) + (255,)
    m16, m24 = grid(MARK16), grid(MARK24)
    icons = [tile(m16, k, 16 * k, ink, clear) for k in (1, 2, 3)]
    for im, n in zip(icons, (16, 32, 48)): im.save(f'{OUT}/web/favicon-{n}.png')
    icons[-1].save(f'{OUT}/web/favicon.ico', append_images=icons[:-1], sizes=[(16, 16), (32, 32), (48, 48)])
    for n, k, size in (('apple-touch-icon', 5, 180), ('icon-192', 5, 192), ('icon-512', 14, 512), ('maskable-512', 10, 512)):
        tile(m24, k, size, paper, ink).save(f'{OUT}/web/{n}.png')
    svg(m16, f'{OUT}/web/favicon.svg')

def device_screens():
    """The real renderer's framebuffers, from the host test."""
    root = os.path.join(HERE, '..', '..')
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run(['test/run.sh', tmp], cwd=root, check=True, stdout=subprocess.DEVNULL)
        for n in ('boot', 'scan', 'terminal', 'sleep'):
            png(~np.array(Image.open(f'{tmp}/{n}.pbm').convert('1')), f'{OUT}/device/{n}.png')

def main():
    for d in ('mascot', 'mark', 'logo', 'web', 'device'): os.makedirs(os.path.join(OUT, d), exist_ok=True)
    poses = {'awake': pose('awake'), 'asleep': pose('asleep'),
             'scan-1': pose('scan', 1), 'scan-2': pose('scan', 2), 'scan-3': pose('scan', 3)}
    y0, y1, x0, x1 = crop(list(poses.values()))
    poses = {n: a[y0:y1, x0:x1] for n, a in poses.items()}
    h, w = poses['awake'].shape
    for n, a in poses.items():
        svg(a, f'{OUT}/mascot/{n}.svg', title=f'x4ssh snail, {n}')
        png(a, f'{OUT}/mascot/{n}.png')
        png(a, f'{OUT}/mascot/{n}@{SCALE}x.png', SCALE)

    for n, src in (('mark-16', MARK16), ('mark-24', MARK24)):
        a = grid(src)
        svg(a, f'{OUT}/mark/{n}.svg')
        svg(a, f'{OUT}/mark/{n}-reversed.svg', PAPER, INK)
        png(a, f'{OUT}/mark/{n}.png')
        png(a, f'{OUT}/mark/{n}@8x.png', 8)
    for n, a in lockups().items():
        svg(a, f'{OUT}/logo/{n}.svg')
        svg(a, f'{OUT}/logo/{n}-reversed.svg', PAPER, INK)
        png(a, f'{OUT}/logo/{n}@4x.png', 4)

    # The snail sits on the screen's centre line; the dish and the z's reach out to its right.
    ys, xs = np.where(poses['awake'][-14:])
    foot_centre = (xs.min() + xs.max()) // 2
    x = SCREEN_W // 2 - foot_centre * SCALE
    y = OFFSET_Y + FIRST_ROW * ROW_H
    last_row = (y + h * SCALE - 1 - OFFSET_Y) // ROW_H
    assert x >= 20 and x + w * SCALE <= SCREEN_W - 20, (x, w)
    icon = grid(MARK24)
    with open(HEADER, 'w') as f:
        f.write('// Generated by design/mascot/build.py, do not edit. Rows MSB-first, padded to a byte; 1 is ink.\n'
                '#pragma once\n#include <cstdint>\n\n'
                f'#define MASCOT_W {w}\n#define MASCOT_H {h}\n#define MASCOT_SCALE {SCALE}\n'
                f'#define MASCOT_X {x}\n#define MASCOT_Y {y}\n'
                f'#define MASCOT_FIRST_ROW {FIRST_ROW}\n#define MASCOT_LAST_ROW {last_row}\n'
                f'#define MASCOT_ICON_W {icon.shape[1]}\n#define MASCOT_ICON_H {icon.shape[0]}\n\n')
        for n, c in (('awake', 'kMascotAwake'), ('asleep', 'kMascotAsleep'),
                     ('scan-1', 'kMascotScan1'), ('scan-2', 'kMascotScan2'), ('scan-3', 'kMascotScan3')):
            f.write(carray(c, poses[n]) + '\n')
        f.write(carray('kMascotIcon', icon))
    web_icons()
    device_screens()
    print(f'mascot {w}x{h} at ({x},{y}), rows {FIRST_ROW}-{last_row}')

if __name__ == '__main__':
    main()
