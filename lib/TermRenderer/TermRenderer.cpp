#include "TermRenderer.h"
#include "TermCell.h"
#include "mascot.h"
#include "term_font_10x20.h"
#include "term_font_ext.h"
#include "term_glyphs.h"

// x4ssh: the codepoint used to be narrowed to uint8_t at the glyph lookup, so everything past ASCII
// drew as '?' or as the wrong letter. Now: ASCII font, drawn box/block glyphs, Latin-1 and the rest
// of the extended table, then '?'.
static const uint8_t* glyphFor(uint16_t cp, uint8_t* scratch) {
  if (cp < 0x80) return TermFont::getGlyph(cp);
  if (TermGlyphs::synth(cp, scratch)) return scratch;
  if (const uint8_t* g = TermFontExt::lookup(cp)) return g;
  return TermFont::getGlyph('?');
}

// 4x4 Bayer dithering matrix (threshold values 0-15)
static const uint8_t kBayer4x4[4][4] = {
    { 0,  8,  2, 10},
    {12,  4, 14,  6},
    { 3, 11,  1,  9},
    {15,  7, 13,  5}
};

void TermRenderer::blitGlyph(int px, int py, const uint8_t* glyph,
                              uint8_t bgBright, bool invertGlyph) {
  uint8_t* fb = _display.getFrameBuffer();
  constexpr int fbStride = DISPLAY_W / 8;  // 100 bytes per row

  for (int gy = 0; gy < TERM_FONT_H; gy++) {
    int fbY = py + gy;
    if (fbY >= DISPLAY_H) break;

    for (int gx = 0; gx < TERM_FONT_W; gx++) {
      int fbX = px + gx;
      if (fbX >= DISPLAY_W) break;

      // Read glyph bit (MSB-first)
      int glyphByteIdx = gy * TermFont::BYTES_PER_ROW + (gx / 8);
      int glyphBitIdx = 7 - (gx % 8);
      bool isGlyphPixel = (pgm_read_byte(&glyph[glyphByteIdx]) >> glyphBitIdx) & 1;

      bool drawBlack;
      if (isGlyphPixel) {
        // Foreground: black normally, white on dark backgrounds
        drawBlack = !invertGlyph;
      } else {
        // Background: Bayer-dithered based on brightness
        // bgBright=255 → all white, bgBright=0 → all black
        int threshold = kBayer4x4[gy & 3][gx & 3];  // 0-15
        int level = (bgBright * 17) >> 8;            // 0-16
        drawBlack = (level <= threshold);
      }

      // Write to framebuffer (bit=1 → white, bit=0 → black)
      int fbByteIdx = fbY * fbStride + (fbX / 8);
      int fbBitIdx = 7 - (fbX % 8);
      if (drawBlack) {
        fb[fbByteIdx] &= ~(1 << fbBitIdx);
      } else {
        fb[fbByteIdx] |= (1 << fbBitIdx);
      }
    }
  }
}

void TermRenderer::renderRow(int row) {
  for (int col = 0; col < TERM_COLS; col++) {
    const TermCell& cell = _buf.cellAt(row, col);
    uint8_t scratch[TermFont::BYTES_PER_GLYPH];
    const uint8_t* glyph = glyphFor(cell.codepoint, scratch);

    // Determine effective background brightness
    uint8_t bgBright = cell.bgBright;
    bool isInverse = (cell.attrs & TermCell::ATTR_INVERSE) != 0;
    if (isInverse) bgBright = 255 - bgBright;

    // Invert glyph when background is dark (for readability)
    bool invertGlyph = bgBright < 128;

    blitGlyph(TERM_OFFSET_X + col * TERM_FONT_W, TERM_OFFSET_Y + row * TERM_FONT_H,
              glyph, bgBright, invertGlyph);
  }
}

void TermRenderer::renderDirty() {
  if (_paged) return;
  if (_art && artCovered()) setArt(nullptr);
  uint32_t dirty = _buf.dirtyRows();

  // Always include the previous cursor row so the old cursor gets erased
  if (_lastCursorRow >= 0) {
    dirty |= (1u << _lastCursorRow);
  }

  if (dirty == 0 && !_statusPending) return;

  // Render all dirty rows into framebuffer (this erases old cursor too)
  for (int row = 0; row < TERM_ROWS; row++) {
    if (dirty & (1u << row)) {
      renderRow(row);
    }
  }

  if (_art) renderArt();

  // Draw cursor at new position
  renderCursor();
  renderStatus();

  // x4ssh: always the full-screen fast refresh (CrossPoint's page-turn path). The windowed one
  // (driver's EXPERIMENTAL displayWindow) is the suspect for the screen flipping back and forth on
  // key presses (reported 2026-11-03), and it saved nothing:
  // the panel waveform takes the same ~420 ms, the extra SPI write ~10 ms.
  _display.displayBuffer(EInkDisplay::FAST_REFRESH);
  _fastRefreshCount++;

  // Periodic full refresh to clear ghosting
  if (_fastRefreshCount >= FULL_REFRESH_INTERVAL) {
    _display.displayBuffer(EInkDisplay::FULL_REFRESH);
    _fastRefreshCount = 0;
  }

  _lastCursorRow = _buf.cursorRow();
  _lastCursorCol = _buf.cursorCol();
  _buf.clearDirty();
}

void TermRenderer::renderFull() {
  _paged = false;
  _buf.markAllDirty();
  for (int row = 0; row < TERM_ROWS; row++) {
    renderRow(row);
  }
  if (_art) renderArt();
  renderCursor();
  renderStatus();
  _display.displayBuffer(EInkDisplay::FULL_REFRESH);
  _fastRefreshCount = 0;
  _lastCursorRow = _buf.cursorRow();
  _lastCursorCol = _buf.cursorCol();
  _buf.clearDirty();
}

void TermRenderer::pageBegin() {
  memset(_display.getFrameBuffer(), 0xFF, STATUS_Y * (DISPLAY_W / 8));
  _paged = true;
}

void TermRenderer::pageLine(int row, const char* text, bool band) {
  for (int col = 0; col < TERM_COLS; col++) {
    uint8_t scratch[TermFont::BYTES_PER_GLYPH];
    const uint8_t c = col && *text ? (uint8_t)*text++ : ' ';  // one cell in, as the status bar's text sits
    blitGlyph(TERM_OFFSET_X + col * TERM_FONT_W, TERM_OFFSET_Y + row * TERM_FONT_H, glyphFor(c, scratch), band ? 0 : 255, band);
  }
}

void TermRenderer::pageEnd() {
  renderStatus();
  _display.displayBuffer(EInkDisplay::FAST_REFRESH);
  _fastRefreshCount++;
}

void TermRenderer::renderStatus() {
  constexpr int y = STATUS_Y;
  memset(_display.getFrameBuffer() + y * (DISPLAY_W / 8), 0x00, (DISPLAY_H - y) * (DISPLAY_W / 8));
  for (int col = 0; col < TERM_COLS; col++) {
    uint8_t scratch[TermFont::BYTES_PER_GLYPH];
    blitGlyph(TERM_OFFSET_X + col * TERM_FONT_W, y, glyphFor(_status[col] ? _status[col] : ' ', scratch), 0, true);
  }
  _statusPending = false;

  // The small snail, mid-bar, unless a long network name reaches it.
  constexpr int ix = (DISPLAY_W - MASCOT_ICON_W) / 2, iy = y + 1;
  constexpr int stride = (MASCOT_ICON_W + 7) / 8;
  if (_art) return;
  for (int col = (ix - TERM_OFFSET_X) / TERM_FONT_W - 1; col <= (ix + MASCOT_ICON_W - TERM_OFFSET_X) / TERM_FONT_W + 1; col++) {
    if (_status[col] && _status[col] != ' ') return;
  }
  uint8_t* fb = _display.getFrameBuffer();
  for (int r = 0; r < MASCOT_ICON_H; r++) {
    for (int c = 0; c < MASCOT_ICON_W; c++) {
      if (!(kMascotIcon[r * stride + c / 8] >> (7 - c % 8) & 1)) continue;
      fb[(iy + r) * (DISPLAY_W / 8) + (ix + c) / 8] |= 1 << (7 - (ix + c) % 8);
    }
  }
}

bool TermRenderer::artCovered() const {
  for (int row = MASCOT_FIRST_ROW; row <= MASCOT_LAST_ROW; row++) {
    for (int col = 0; col < TERM_COLS; col++) {
      const uint16_t cp = _buf.cellAt(row, col).codepoint;
      if (cp && cp != ' ') return true;
    }
  }
  return false;
}

void TermRenderer::renderArt() {
  constexpr int stride = (MASCOT_W + 7) / 8;
  uint8_t* fb = _display.getFrameBuffer();
  for (int r = 0; r < MASCOT_H; r++) {
    for (int c = 0; c < MASCOT_W; c++) {
      if (!(_art[r * stride + c / 8] >> (7 - c % 8) & 1)) continue;
      for (int dy = 0; dy < MASCOT_SCALE; dy++) {
        for (int dx = 0; dx < MASCOT_SCALE; dx++) {
          const int x = MASCOT_X + c * MASCOT_SCALE + dx, y = MASCOT_Y + r * MASCOT_SCALE + dy;
          fb[y * (DISPLAY_W / 8) + x / 8] &= ~(1 << (7 - x % 8));
        }
      }
    }
  }
}

void TermRenderer::renderCursor() {
  if (!_cursorVisible) return;

  // Draw cursor as inverted block at current position
  int row = _buf.cursorRow();
  int col = _buf.cursorCol();

  if (col >= TERM_COLS) col = TERM_COLS - 1;

  const TermCell& cell = _buf.cellAt(row, col);
  uint8_t scratch[TermFont::BYTES_PER_GLYPH];
  const uint8_t* glyph = glyphFor(cell.codepoint, scratch);

  // Cursor: invert the cell's effective background
  uint8_t bgBright = cell.bgBright;
  bool isInverse = (cell.attrs & TermCell::ATTR_INVERSE) != 0;
  if (isInverse) bgBright = 255 - bgBright;
  // Flip for cursor
  bgBright = 255 - bgBright;
  bool invertGlyph = bgBright < 128;

  blitGlyph(TERM_OFFSET_X + col * TERM_FONT_W, TERM_OFFSET_Y + row * TERM_FONT_H,
            glyph, bgBright, invertGlyph);
}
