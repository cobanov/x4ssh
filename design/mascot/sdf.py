# Tiny numpy SDF raymarcher for 1-bit mascots: orthographic camera, soft light, AO, materials.
import numpy as np

def length(v): return np.sqrt((v * v).sum(-1))
def sphere(p, c, r): return length(p - np.array(c)) - r
def ellipsoid(p, c, r):
    q = (p - np.array(c)) / np.array(r)
    k0 = length(q); k1 = length(q / np.array(r))
    return k0 * (k0 - 1.0) / np.maximum(k1, 1e-6)
def capsule(p, a, b, r):
    a, b = np.array(a), np.array(b)
    pa, ba = p - a, b - a
    h = np.clip((pa * ba).sum(-1) / (ba * ba).sum(), 0, 1)
    return length(pa - ba * h[..., None]) - r
def smin(a, b, k):
    h = np.clip(0.5 + 0.5 * (b - a) / k, 0, 1)
    return b * (1 - h) + a * h - k * h * (1 - h)

def rot(yaw, pitch):
    cy, sy, cp, sp = np.cos(yaw), np.sin(yaw), np.cos(pitch), np.sin(pitch)
    Ry = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])
    Rx = np.array([[1, 0, 0], [0, cp, -sp], [0, sp, cp]])
    return Ry @ Rx

def render(scene, w, h, center, scale, yaw, pitch, light=(-0.5, 0.8, 0.6)):
    """scene(p) -> (dist, material). Returns depth, material, shade (0..1), normal-ish."""
    R = rot(yaw, pitch)
    right, up, fwd = R[:, 0], R[:, 1], -R[:, 2]
    xs = (np.arange(w) + 0.5 - w / 2) * scale
    ys = (h / 2 - np.arange(h) - 0.5) * scale
    X, Y = np.meshgrid(xs, ys)
    ro = np.array(center) + X[..., None] * right + Y[..., None] * up - fwd * 10
    rd = np.broadcast_to(fwd, ro.shape)
    t = np.zeros((h, w)); hit = np.zeros((h, w), bool)
    for _ in range(160):
        d, _m = scene(ro + rd * t[..., None])
        hit |= d < 1e-3
        t = np.where(hit, t, t + np.maximum(d * 0.9, 1e-3))
    hit &= t < 20
    p = ro + rd * t[..., None]
    d, mat = scene(p)
    e = 1e-3
    n = np.stack([scene(p + np.array([e, 0, 0]))[0] - scene(p - np.array([e, 0, 0]))[0],
                  scene(p + np.array([0, e, 0]))[0] - scene(p - np.array([0, e, 0]))[0],
                  scene(p + np.array([0, 0, e]))[0] - scene(p - np.array([0, 0, e]))[0]], -1)
    n /= np.maximum(length(n)[..., None], 1e-9)
    L = np.array(light, float); L /= np.linalg.norm(L)
    diff = np.clip((n * L).sum(-1), 0, 1)
    # soft shadow toward the light
    sh = np.ones((h, w)); ts = np.full((h, w), 0.02)
    for _ in range(48):
        dd, _m = scene(p + n * 0.01 + L * ts[..., None])
        sh = np.minimum(sh, np.clip(10 * dd / ts, 0, 1))
        ts += np.clip(dd, 0.01, 0.2)
    # ambient occlusion
    ao = np.ones((h, w))
    for i in range(1, 6):
        hh = 0.06 * i
        dd, _m = scene(p + n * hh)
        ao -= (hh - dd) * (0.5 ** i) * 4
    ao = np.clip(ao, 0, 1)
    # specular (for glossy eyes)
    hv = L - rd; hv /= length(hv)[..., None]
    spec = np.clip((n * hv).sum(-1), 0, 1) ** 40
    shade = np.clip(0.16 + 0.84 * diff * (0.55 + 0.45 * sh), 0, 1) * (0.5 + 0.5 * ao)
    return dict(hit=hit, depth=np.where(hit, t, np.inf), mat=np.where(hit, mat, -1), shade=shade, spec=spec,
                ao=ao, shadow=sh, n=n, p=p)

BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16 + 1 / 32

def neighbours(a, fillv):
    p = np.pad(a, 1, constant_values=fillv)
    return p[1:-1, 2:], p[1:-1, :-2], p[2:, 1:-1], p[:-2, 1:-1]

def edges(r, skip):
    """Outline pixels: the nearer side of every silhouette, crease and material boundary."""
    dep, mat = r['depth'], r['mat']
    edge = np.zeros(mat.shape, bool)
    solid = (mat >= 0) & (mat != skip)
    for md, dd in zip(neighbours(mat, -1), neighbours(dep, np.inf)):
        front = dep < dd
        with np.errstate(invalid='ignore'):
            gap = np.abs(dep - dd) > r['crease']
        edge |= solid & front & ((mat != md) | gap)
    return edge

def project(p, w, h, center, scale, yaw, pitch):
    R = rot(yaw, pitch)
    v = np.array(p, float) - np.array(center)
    return w / 2 + (v @ R[:, 0]) / scale, h / 2 - (v @ R[:, 1]) / scale
