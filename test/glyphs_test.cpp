// Host test for the drawn box/block glyphs: prints a few and checks key pixels. Run with test/run.sh
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include "term_glyphs.h"

static bool on(const uint8_t* g, int x, int y) { return g[y * 2 + x / 8] & (0x80 >> (x % 8)); }

static void show(uint16_t cp) {
  uint8_t g[40];
  assert(TermGlyphs::synth(cp, g));
  printf("U+%04X\n", cp);
  for (int y = 0; y < 20; y++) {
    for (int x = 0; x < 10; x++) putchar(on(g, x, y) ? '#' : '.');
    putchar('\n');
  }
}

int main() {
  uint8_t g[40];
  assert(!TermGlyphs::synth('A', g));
  assert(TermGlyphs::synth(0x2588, g));  // full block: every pixel
  for (int y = 0; y < 20; y++)
    for (int x = 0; x < 10; x++) assert(on(g, x, y));
  TermGlyphs::synth(0x2500, g);          // ─ spans the width on rows 9-10 only
  assert(on(g, 0, 9) && on(g, 9, 10) && !on(g, 0, 8) && !on(g, 4, 0));
  TermGlyphs::synth(0x2554, g);          // ╔ outer corner at (2,7), inner corner at (6,11), nothing up-left
  assert(on(g, 2, 7) && on(g, 9, 7) && on(g, 2, 19) && on(g, 6, 11) && on(g, 6, 19));
  assert(!on(g, 0, 0) && !on(g, 0, 7) && !on(g, 2, 6) && !on(g, 6, 9) && !on(g, 4, 9));
  TermGlyphs::synth(0x2550, g);          // ═ two full-width lines
  assert(on(g, 0, 7) && on(g, 9, 8) && on(g, 0, 11) && on(g, 9, 12) && !on(g, 5, 9));
  TermGlyphs::synth(0x2580, g);          // ▀ top half
  assert(on(g, 0, 0) && on(g, 9, 9) && !on(g, 0, 10));
  if (getenv("SHOW")) { show(0x2554); show(0x255D); show(0x2566); show(0x253C); }
  puts("glyphs ok");
}
