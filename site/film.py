# PBM frames to two-colour PNGs in the kit's ink and paper, plus the frame table the hero reads.
#   uvx -q --with pillow python site/film.py <dir written by film>
import json, os, sys
from PIL import Image

src, site = sys.argv[1], os.path.dirname(os.path.abspath(__file__))
out = os.path.join(site, 'public', 'film')
os.makedirs(out, exist_ok=True)
frames = []
for line in open(os.path.join(src, 'index.txt')):
    name, kind, hold, key, mark = line.split()
    bw = Image.open(os.path.join(src, name + '.pbm')).convert('1')
    p = Image.frombytes('P', bw.size, bytes(1 if v else 0 for v in bw.convert("L").tobytes()))
    p.putpalette([0x22, 0x22, 0x22, 0xE2, 0xE0, 0xD8])
    p.save(os.path.join(out, name + '.png'), optimize=True, bits=1)
    frames.append({'kind': kind, 'hold': int(hold), 'key': None if key == '-' else key, 'mark': None if mark == '-' else mark})
assert frames[0]['mark'] == 'asleep' and frames[-1]['mark'] == 'sleep'
json.dump(frames, open(os.path.join(site, 'hero', 'film.json'), 'w'), indent=0)
print(len(frames), 'frames,', sum(f['hold'] for f in frames) / 1000, 's held')
