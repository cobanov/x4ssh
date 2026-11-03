# The share image, from the kit: the logo, the headline, the snail on a piece of the panel, the status bar.
#   uvx -q --with pillow python site/og.py
import os
from PIL import Image, ImageDraw, ImageFont

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
INK, PAPER = (0x22, 0x22, 0x22), (0xE2, 0xE0, 0xD8)
W, H, BAND = 1200, 630, 56
BOLD = os.path.join(root, 'design/fonts/DejaVuSansMono-Bold.ttf')
og = Image.new('RGB', (W, H), INK)
d = ImageDraw.Draw(og)


def stepped(w, h, fill):
    # 4px steps in the corners, as the page draws them
    m = Image.new('1', (w, h), 1)
    p = ImageDraw.Draw(m)
    for i, cut in enumerate((12, 8, 4)):
        for x0, y0 in ((0, i * 4), (w - cut, i * 4), (0, h - i * 4 - 4), (w - cut, h - i * 4 - 4)):
            p.rectangle((x0, y0, x0 + cut - 1, y0 + 3), fill=0)
    return Image.new('RGB', (w, h), fill) if fill else None, m


def dither(w, h):
    t = Image.new('RGB', (w, h), INK)
    for y in range(0, h, 2):
        for x in range(0, w, 2):
            if (x // 2 + y // 2) % 2 == 0:
                t.paste(PAPER, (x, y, x + 2, y + 2))
    return t


# The logo's two inks swapped, 4 art pixels to the pixel
logo = Image.open(os.path.join(root, 'design/assets/logo/horizontal@4x.png')).convert('RGB')
logo.putdata([PAPER if p == INK else INK for p in logo.getdata()])
og.paste(logo, (60, 108))

d.text((60, 232), 'SSH on a pocket\ne-ink reader', font=ImageFont.truetype(BOLD, 56), fill=PAPER, spacing=12)
x = 60 + int(d.textlength('e-ink reader', font=ImageFont.truetype(BOLD, 56))) + 8
d.rectangle((x, 312, x + 32, 368), fill=PAPER)
d.text((60, 408), 'An SSH terminal for the Xteink X4', font=ImageFont.truetype(BOLD.replace('-Bold', ''), 26), fill=PAPER)

snail = Image.open(os.path.join(root, 'design/assets/mascot/scan-3@3x.png')).convert('RGB')
tw, th = snail.width + 40, snail.height + 40
tx, ty = W - 60 - tw - 12, 116
_, mask = stepped(tw, th, None)
og.paste(dither(tw, th), (tx + 12, ty + 12), mask)
tile, _ = stepped(tw, th, PAPER)
tile.paste(snail, (20, 20))
og.paste(tile, (tx, ty), mask)

# Paper going up from the ink into the status bar, a dither band at a time
fade = Image.open(os.path.join(root, 'site/public/img/fade.png')).convert('RGB').transpose(Image.FLIP_TOP_BOTTOM)
fade = fade.resize((fade.width * 2, fade.height * 2), Image.NEAREST)
for x in range(0, W, fade.width):
    og.paste(fade, (x, H - BAND - fade.height))
d.rectangle((0, H - BAND, W, H), fill=PAPER)
f = ImageFont.truetype(BOLD, 24)
y = H - BAND + (BAND - 30) // 2
d.text((60, y), 'x4ssh.cobanov.dev', font=f, fill=INK)
d.text((W - 60 - d.textlength('99% | 23:58', font=f), y), '99% | 23:58', font=f, fill=INK)

og.convert('P', palette=Image.ADAPTIVE, colors=16).save(os.path.join(root, 'site/public/img/og.png'), optimize=True)
