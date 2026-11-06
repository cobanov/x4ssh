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

  // The panel shows the cursor somewhere else, or shows it when it shouldn't. No text changed, so nothing else redraws it.
  bool cursorMoved() const {
    if (_cursorVisible != _drawnCursorVisible) return true;
    return _cursorVisible && (_buf.cursorRow() != _lastCursorRow || _buf.cursorCol() != _lastCursorCol);
  }

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

  // A page of text over the terminal. The terminal keeps taking text underneath, undrawn, until renderFull.
  void pageBegin();
  void pageLine(int row, const char* text, bool band = false);  // band: paper on ink, like the status bar
  void pageEnd();
  bool paged() const { return _paged; }

 private:
  EInkDisplay& _display;
  TermBuffer& _buf;
  int _fastRefreshCount = 0;
  int _lastCursorRow = -1;
  int _lastCursorCol = -1;
  bool _cursorVisible = true;
  bool _drawnCursorVisible = false;
  uint16_t _status[TERM_COLS] = {};
  bool _statusPending = false;
  const uint8_t* _art = nullptr;
  bool _paged = false;

  void renderStatus();
  void renderArt();
  bool artCovered() const;

  void renderRow(int row);
  void drawCell(int row, int col, bool cursor);
  void blitGlyph(int px, int py, const uint8_t* glyph, uint8_t bgBright, bool invertGlyph, uint8_t attrs = 0);
};
