# The page's dither and the hero's snail, cut from the kit.
#   uvx -q --with pillow python site/tiles.py
import os
from PIL import Image

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
out = os.path.join(root, 'site/public/img')
INK, PAPER = (0x22, 0x22, 0x22), (0xE2, 0xE0, 0xD8)
# The kit's 2x2 patterns, as the pixels of a tile that are paper
D25, D50, D75 = {(0, 0)}, {(0, 0), (1, 1)}, {(0, 0), (1, 1), (1, 0)}


def rows(*bands):
    tile = Image.new('RGB', (2, 4 * len(bands)), INK)
    for y in range(tile.height):
        for x in range(2):
            if (x, y % 2) in bands[y // 4]:
                tile.putpixel((x, y), PAPER)
    return tile


rows(D75, D50, D25).save(os.path.join(out, 'fade.png'), optimize=True)
rows(D50).crop((0, 0, 2, 2)).save(os.path.join(out, 'd50.png'), optimize=True)

# The dish sweeping out and back, then awake: the frame the page rests on once the sweep stops
poses = ['scan-1', 'scan-2', 'scan-3', 'scan-2', 'awake']
frames = [Image.open(os.path.join(root, f'design/assets/mascot/{p}.png')).convert('RGB') for p in poses]
sheet = Image.new('RGB', (149 * len(frames), 89))
for i, f in enumerate(frames):
    sheet.paste(f, (149 * i, 0))
sheet.convert('P', palette=Image.ADAPTIVE, colors=2).save(os.path.join(out, 'snail.png'), optimize=True)
