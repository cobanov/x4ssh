#pragma once
#include "TermBuffer.h"
#include "term_config.h"
#include <EInkDisplay.h>
#include <cstring>

class TermRenderer {
 public:
  TermRenderer(EInkDisplay& display, TermBuffer& buf)
      : _display(display), _buf(buf) {}

  // Render all dirty rows and refresh display
  void renderDirty();

  // Force full-screen render + full refresh (clear ghosting)
  void renderFull();

  // Render cursor at current position (XOR block)
  void renderCursor();

  // Cursor visibility (set from VtParser's DECTCEM state)
  void setCursorVisible(bool v) { _cursorVisible = v; }

  // Bottom status bar (TERM_COLS codepoints), white on black under the terminal rows.
  // A changed bar is drawn with the next renderDirty, even when no terminal row changed.
  void setStatus(const uint16_t* cells) {
    if (memcmp(cells, _status, sizeof _status) == 0) return;
    memcpy(_status, cells, sizeof _status);
    _statusPending = true;
  }
  bool statusPending() const { return _statusPending; }

  // A MASCOT_W x MASCOT_H bitmap (mascot.h) drawn over the terminal until text reaches its rows.
  // While there is none, the small snail sits in the status bar instead. nullptr takes it down.
  void setArt(const uint8_t* bits) {
    if (bits == _art) return;
    _art = bits;
    _buf.markAllDirty();
  }
  bool hasArt() const { return _art != nullptr; }

 private:
  EInkDisplay& _display;
  TermBuffer& _buf;
  int _fastRefreshCount = 0;
  int _lastCursorRow = -1;
  int _lastCursorCol = -1;
  bool _cursorVisible = true;
  uint16_t _status[TERM_COLS] = {};
  bool _statusPending = false;
  const uint8_t* _art = nullptr;

  void renderStatus();
  void renderArt();
  bool artCovered() const;

  void renderRow(int row);
  void blitGlyph(int px, int py, const uint8_t* glyph, uint8_t bgBright, bool invertGlyph);
};
