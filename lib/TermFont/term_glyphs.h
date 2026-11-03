#pragma once
// Box drawing (U+2500-257F) and block elements (U+2580-259F), drawn into a 10x20 cell in the
// font's own format (2 bytes per row, MSB first) so the renderer blits them like any glyph.
// x4ssh: X4Term's font only had ASCII; tmux borders, htop bars and figlet art came out as '?'.
#include <cstdint>
#include <cstring>

namespace TermGlyphs {

// Arms per box character: up << 6 | right << 4 | down << 2 | left; 0 none, 1 light, 2 heavy, 3 double.
// Generated from the Unicode chart; dashed variants draw solid, arcs as square corners.
static const uint8_t kBox[128] = {
    0x11, 0x22, 0x44, 0x88, 0x11, 0x22, 0x44, 0x88, 0x11, 0x22, 0x44, 0x88, 0x14, 0x24, 0x18, 0x28,  // U+2500
    0x05, 0x06, 0x09, 0x0A, 0x50, 0x60, 0x90, 0xA0, 0x41, 0x42, 0x81, 0x82, 0x54, 0x64, 0x94, 0x58,  // U+2510
    0x98, 0xA4, 0x68, 0xA8, 0x45, 0x46, 0x85, 0x49, 0x89, 0x86, 0x4A, 0x8A, 0x15, 0x16, 0x25, 0x26,  // U+2520
    0x19, 0x1A, 0x29, 0x2A, 0x51, 0x52, 0x61, 0x62, 0x91, 0x92, 0xA1, 0xA2, 0x55, 0x56, 0x65, 0x66,  // U+2530
    0x95, 0x59, 0x99, 0x96, 0xA5, 0x5A, 0x69, 0xA6, 0x6A, 0x9A, 0xA9, 0xAA, 0x11, 0x22, 0x44, 0x88,  // U+2540
    0x33, 0xCC, 0x34, 0x1C, 0x3C, 0x07, 0x0D, 0x0F, 0x70, 0xD0, 0xF0, 0x43, 0xC1, 0xC3, 0x74, 0xDC,  // U+2550
    0xFC, 0x47, 0xCD, 0xCF, 0x37, 0x1D, 0x3F, 0x73, 0xD1, 0xF3, 0x77, 0xDD, 0xFF, 0x14, 0x05, 0x41,  // U+2560
    0x50, 0x00, 0x00, 0x00, 0x01, 0x40, 0x10, 0x04, 0x02, 0x80, 0x20, 0x08, 0x21, 0x48, 0x12, 0x84,  // U+2570
};

struct Cell {
  uint8_t* g;
  void px(int x, int y) {
    if (x >= 0 && x < 10 && y >= 0 && y < 20) g[y * 2 + x / 8] |= 0x80 >> (x % 8);
  }
  void rect(int x0, int x1, int y0, int y1) {
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++) px(x, y);
  }
};

// Light lines are 2 px, heavy 4 px; double is two 2 px lines (x 2-3 and 6-7, y 7-8 and 11-12).
inline void drawBox(Cell c, uint8_t arms, uint16_t cp) {
  const int U = arms >> 6, R = (arms >> 4) & 3, D = (arms >> 2) & 3, L = arms & 3;
  const int V = U > D ? U : D, H = L > R ? L : R;  // heaviest vertical / horizontal arm
  if (cp >= 0x2571 && cp <= 0x2573) {  // diagonals
    for (int y = 0; y < 20; y++) {
      if (cp != 0x2572) c.rect(9 - y / 2, 9 - y / 2, y, y);
      if (cp != 0x2571) c.rect(y / 2, y / 2, y, y);
    }
    return;
  }
  for (int side = 0; side < 2; side++) {  // 0: up/left, 1: down/right
    const int v = side ? D : U, h = side ? R : L;
    if (v == 1 || v == 2) {
      const int x0 = v == 1 ? 4 : 3, x1 = v == 1 ? 5 : 6;
      const int lo = H == 3 ? 11 : H == 2 ? 8 : 9, hi = H == 3 ? 8 : H == 2 ? 11 : 10;
      side ? c.rect(x0, x1, lo, 19) : c.rect(x0, x1, 0, hi);
    } else if (v == 3) {
      for (int line = 0; line < 2; line++) {  // left line meets L, right line meets R
        const int p = line ? R : L, x = line ? 6 : 2;
        const bool shortLine = p == 3, longLine = p == 0 && H == 3;
        if (side) c.rect(x, x + 1, shortLine ? 11 : longLine ? 7 : 9, 19);
        else c.rect(x, x + 1, 0, shortLine ? 8 : longLine ? 12 : 10);
      }
    }
    if (h == 1 || h == 2) {
      const int y0 = h == 1 ? 9 : 8, y1 = h == 1 ? 10 : 11;
      const int lo = V == 3 ? 6 : V == 2 ? 3 : 4, hi = V == 3 ? 3 : V == 2 ? 6 : 5;
      side ? c.rect(lo, 9, y0, y1) : c.rect(0, hi, y0, y1);
    } else if (h == 3) {
      for (int line = 0; line < 2; line++) {  // top line meets U, bottom line meets D
        const int p = line ? D : U, y = line ? 11 : 7;
        const bool shortLine = p == 3, longLine = p == 0 && V == 3;
        if (side) c.rect(shortLine ? 6 : longLine ? 2 : 4, 9, y, y + 1);
        else c.rect(0, shortLine ? 3 : longLine ? 7 : 5, y, y + 1);
      }
    }
  }
}

inline void drawBlock(Cell c, uint16_t cp) {
  static const uint8_t kQuad[10] = {4, 8, 1, 13, 9, 7, 11, 2, 6, 14};  // U+2596..259F, UL=1 UR=2 LL=4 LR=8
  if (cp == 0x2580) c.rect(0, 9, 0, 9);
  else if (cp <= 0x2588) c.rect(0, 9, 20 - (cp - 0x2580) * 20 / 8, 19);   // lower n/8 .. full
  else if (cp <= 0x258F) c.rect(0, (0x2590 - cp) * 10 / 8 - 1, 0, 19);   // left 7/8 .. 1/8
  else if (cp == 0x2590) c.rect(5, 9, 0, 19);
  else if (cp <= 0x2593) {                                              // shades 25/50/75 %
    for (int y = 0; y < 20; y++)
      for (int x = 0; x < 10; x++) {
        const bool on = cp == 0x2591 ? (x % 2 == 0 && y % 2 == 0) : cp == 0x2592 ? (x + y) % 2 == 0
                                                                                 : !(x % 2 == 1 && y % 2 == 1);
        if (on) c.px(x, y);
      }
  } else if (cp == 0x2594) c.rect(0, 9, 0, 1);
  else if (cp == 0x2595) c.rect(8, 9, 0, 19);
  else {
    const uint8_t q = kQuad[cp - 0x2596];
    if (q & 1) c.rect(0, 4, 0, 9);
    if (q & 2) c.rect(5, 9, 0, 9);
    if (q & 4) c.rect(0, 4, 10, 19);
    if (q & 8) c.rect(5, 9, 10, 19);
  }
}

// Fills `out` (40 bytes) for a box or block codepoint; false for anything else.
inline bool synth(uint16_t cp, uint8_t* out) {
  if (cp < 0x2500 || cp > 0x259F) return false;
  memset(out, 0, 40);
  if (cp < 0x2580) drawBox(Cell{out}, kBox[cp - 0x2500], cp);
  else drawBlock(Cell{out}, cp);
  return true;
}

}  // namespace TermGlyphs
