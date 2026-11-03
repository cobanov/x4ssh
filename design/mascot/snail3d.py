# x4ssh mascot: a snail modelled with SDFs, lit, posterized to dither bands, outlined from depth.
#   uvx --with pillow --with numpy python snail3d.py OUT_DIR
# Writes <pose>.npy (bool ink array) per pose and a preview sheet.
import sys, os, math, numpy as np
from sdf import *
from PIL import Image

BODY, SHELL, TIP, GROUND = 1, 2, 4, 0
W, H = 176, 100
YAW, PITCH, SCALE = 0.46, -0.2, 0.034
R = rot(YAW, PITCH)
RIGHT, UP, BACK = R[:, 0], R[:, 1], R[:, 2]
CAM = dict(center=tuple(np.array([0.36, 1.02, 0]) + RIGHT * (W - 132) / 2 * SCALE), scale=SCALE, yaw=YAW, pitch=PITCH)
HEAD = np.array([1.3, 1.14, 0.04]); HR = 0.56
SHELL_C = np.array([-0.16, 1.08, -0.04]); PT = 0.25

POSES = {
    'awake':  dict(t1=(0.96, 2.28, -0.26), t2=(1.76, 2.24, 0.3), tip=0.135, turn=0.10),
    'asleep': dict(t1=(1.07, 1.80, -0.17), t2=(1.60, 1.78, 0.2), tip=0.105, turn=0.10),
    'scan':   dict(t1=(1.20, 2.30, -0.20), t2=(1.98, 2.12, 0.2), tip=0.135, turn=0.30),
}

HAND = HEAD + RIGHT * 0.95 + UP * -0.36 + BACK * 0.5   # the scan pose's little hand

def face_axes(turn):
    f = BACK * math.cos(turn) + RIGHT * math.sin(turn)
    return f, RIGHT * math.cos(turn) - BACK * math.sin(turn)

def eyes_at(turn):
    f, side = face_axes(turn)
    return [HEAD + f * 0.46 + side * dx + UP * 0.06 for dx in (-0.25, 0.25)]

def make(name):
    P = POSES[name]
    t1a, t2a = (1.12, 1.56, -0.12), (1.5, 1.56, 0.16)
    def scene(p):
        y = p[..., 1]
        foot = ellipsoid(p, (0.08, 0.17, 0), (1.8, 0.27, 0.52))
        neck = capsule(p, (1.05, 0.25, 0), (1.25, 0.95, 0.02), 0.38)
        head = sphere(p, tuple(HEAD), HR)
        body = smin(smin(foot, neck, 0.3), head, 0.25)
        ten = np.minimum(capsule(p, t1a, P['t1'], 0.065), capsule(p, t2a, P['t2'], 0.065))
        body = smin(body, ten, 0.12)
        tips = np.minimum(sphere(p, P['t1'], P['tip']), sphere(p, P['t2'], P['tip']))
        if name == 'scan':
            arm = capsule(p, tuple(HEAD + UP * -0.62 + RIGHT * 0.25 + BACK * 0.3), tuple(HAND), 0.11)
            body = smin(body, smin(arm, sphere(p, tuple(HAND), 0.17), 0.05), 0.1)
        q = p - SHELL_C
        shell = ellipsoid(p, tuple(SHELL_C), (0.98, 0.95, 0.62))
        rho = np.sqrt(q[..., 0] ** 2 + q[..., 1] ** 2)
        ph = np.mod(rho + np.arctan2(q[..., 1], q[..., 0]) / (2 * np.pi) * PT, PT) / PT
        shell = shell + np.exp(-((ph - 0.5) / 0.13) ** 2) * 0.04 * (rho > 0.07)
        ground = y + 0.09
        d = np.minimum.reduce([body, shell, tips, ground])
        return d, np.select([d == ground, d == shell, d == tips], [GROUND, SHELL, TIP], BODY)
    return scene

def bands(r):
    s, mat = r['shade'], r['mat']
    h, w = s.shape
    yy, xx = np.mgrid[0:h, 0:w]
    d25 = (xx % 2 == 0) & (yy % 2 == 0)
    d50 = (xx + yy) % 2 == 0
    d75 = ~((xx % 2 == 1) & (yy % 2 == 1))
    lum = s ** 0.62
    ink = np.select([lum > 0.74, lum > 0.6, lum > 0.46, lum > 0.34], [False, d25, d50, d75], True)
    q = r['p'] - SHELL_C   # shell: a continuous spiral line drawn over the shading
    rho = np.sqrt(q[..., 0] ** 2 + q[..., 1] ** 2)
    ph = np.mod(rho + np.arctan2(q[..., 1], q[..., 0]) / (2 * np.pi) * PT, PT) / PT
    ink = np.where((mat == SHELL) & (np.abs(ph - 0.5) < 0.1) & (rho > 0.07), True, ink)
    ink = np.where(mat == GROUND, False, ink)
    ink = np.where(mat == TIP, r['spec'] < 0.9, ink)
    return np.where(mat < 0, False, ink)

def put(a, rows, cx, cy, mask=None):
    rows = rows.strip('\n').split('\n')
    ox, oy = cx - len(rows[0]) // 2, cy - len(rows) // 2
    for j, row in enumerate(rows):
        for i, ch in enumerate(row):
            if mask is not None and not mask[oy + j, ox + i]: continue
            if ch == '#': a[oy + j, ox + i] = True
            elif ch == 'o': a[oy + j, ox + i] = False

EYE_OPEN = '''
ooo###ooo
oo#####oo
o##oo###o
o##oo###o
o#######o
o#######o
o#####o#o
oo#####oo
ooo###ooo'''
EYE_SHUT = '''
ooooooooo
o#ooooo#o
o#ooooo#o
oo#ooo#oo
ooo###ooo
ooooooooo'''
BLUSH = '''
.#.#.#.
#.#.#.#
.#.#.#.'''

def edge(m):
    p = np.pad(m, 1)
    return m & ~(p[:-2, 1:-1] & p[2:, 1:-1] & p[1:-1, :-2] & p[1:-1, 2:])

def stroke(a, p0, p1, width=1):
    n = int(max(abs(p1[0] - p0[0]), abs(p1[1] - p0[1]))) + 1
    for t in np.linspace(0, 1, n * 2):
        x, y = int(round(p0[0] + (p1[0] - p0[0]) * t)), int(round(p0[1] + (p1[1] - p0[1]) * t))
        a[y:y + width, x:x + width] = True

def dish(a, hand, arcs):
    """Satellite dish on a pole, facing up and right: rim ellipse, shaded bowl behind it, feed and ball."""
    th = math.radians(38)
    cx, cy = hand[0] + 10, hand[1] - 27
    yy, xx = np.mgrid[0:H, 0:W]
    dx, dy = xx - cx, cy - yy
    u, v = dx * math.cos(th) + dy * math.sin(th), -dx * math.sin(th) + dy * math.cos(th)
    rim = (u / 5.0) ** 2 + (v / 15.0) ** 2 <= 1
    bowl = (u <= 0) & ((u / 11.0) ** 2 + (v / 15.0) ** 2 <= 1)
    back = (cx - 10 * math.cos(th), cy + 10 * math.sin(th))
    stroke(a, hand, back, 2)
    a[rim | bowl] = False
    d50, d75 = (xx + yy) % 2 == 0, ~((xx % 2 == 1) & (yy % 2 == 1))
    a |= bowl & ~rim & np.where(v < -3, d75, d50)
    a |= rim & (u < -1) & (v > 2) & (xx % 2 == 0) & (yy % 2 == 0)
    a |= edge(rim | bowl) | edge(rim)
    tip = (round(cx + 12 * math.cos(th)), round(cy - 12 * math.sin(th)))
    stroke(a, (cx, cy), tip)
    a |= (xx - tip[0]) ** 2 + (yy - tip[1]) ** 2 <= 5
    for r in (9, 15, 21)[:arcs]:
        for t in np.linspace(-0.62, 0.62, 240):
            x, y = int(round(tip[0] + r * math.cos(th + t))), int(round(tip[1] - r * math.sin(th + t)))
            a[y, x] = True

def decals(a, name, r, arcs=3):
    Pj = lambda v: tuple(int(round(c)) for c in project(v, W, H, **CAM))
    e1, e2 = (Pj(e) for e in eyes_at(POSES[name]['turn']))
    mx, my = (e1[0] + e2[0]) // 2, (e1[1] + e2[1]) // 2 + 8
    for ex, ey in (e1, e2):
        put(a, EYE_SHUT if name == 'asleep' else EYE_OPEN, ex, ey + (1 if name == 'asleep' else 0))
    put(a, {'awake': 'ooooooo\no#ooo#o\noo###oo\nooooooo',
            'asleep': 'ooooo\no###o\nooooo',
            'scan': 'ooooo\noo#oo\no#o#o\noo#oo\nooooo'}[name], mx, my)
    for bx, by in ((e1[0] - 5, e1[1] + 8), (e2[0] + 5, e2[1] + 8)):
        put(a, BLUSH, bx, by, r['mat'] == BODY)
    yy, xx = np.mgrid[0:H, 0:W]   # contact shadow, kept clear of the foot's outline
    sh = (((xx - 58) / 50.0) ** 2 + ((yy - 88) / 3.2) ** 2 < 1) & ((xx + yy) % 2 == 0) & ~a
    a |= sh & ~np.maximum.accumulate(a[::-1], axis=0)[::-1]
    if name == 'asleep':
        for (zx, zy, n) in ((103, 27, 5), (109, 16, 6), (117, 3, 8)):
            for i in range(n):
                for (y_, x_) in ((zy, zx + i), (zy + n - 1, zx + i), (zy + i, zx + n - 1 - i)):
                    a[y_, x_] = True
    if name == 'scan':
        dish(a, Pj(HAND), arcs)

def pose(name, arcs=3):
    r = render(make(name), W, H, light=tuple(BACK * 0.9 + UP * 0.6 - RIGHT * 0.12), **CAM)
    r['crease'] = 0.3
    a = bands(r) | edges(r, GROUND)
    decals(a, name, r, arcs)
    return a

def preview(ims, path, k=4):
    gap = 12
    sheet = Image.new('RGB', ((W * k + gap) * len(ims) - gap, H * k), (120, 120, 120))
    for i, a in enumerate(ims):
        big = np.kron(a, np.ones((k, k), bool))
        rgb = np.where(big[..., None], np.array([34, 34, 34], np.uint8), np.array([226, 224, 216], np.uint8))
        sheet.paste(Image.fromarray(rgb.astype(np.uint8)), (i * (W * k + gap), 0))
    sheet.save(path)

if __name__ == '__main__':
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    ims = {n: pose(n) for n in POSES}
    for n, a in ims.items(): np.save(f'{out}/{n}.npy', a)
    preview(list(ims.values()), f'{out}/poses.png', 3)
