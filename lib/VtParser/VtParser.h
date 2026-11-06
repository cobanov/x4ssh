#pragma once
#include "TermBuffer.h"
#include <cstdint>

class VtParser {
 public:
  // Undoes whatever modes a dropped session left on. The scroll region's reset homes the cursor, hence the save around it.
  static constexpr const char* kSessionEnd = "\033[0m\033(B\033[?1049l\0337\033[r\0338\033[?1l\033[?25h";

  VtParser(TermBuffer& buf) : _buf(buf) {}

  // Feed one byte from serial input
  void feed(uint8_t byte);

  // Cursor visibility (controlled by DECTCEM ?25h/l)
  bool cursorVisible() const { return _cursorVisible; }

  // DECCKM (?1h/l): arrows send ESC O x instead of ESC [ x
  bool appCursor() const { return _appCursor; }

  // Where answers to the host's queries (DSR, DA) go: the SSH channel, not the USB serial
  void onReply(void (*reply)(const char*)) { _reply = reply; }

 private:
  TermBuffer& _buf;

  enum class State {
    Ground,
    Escape,
    CsiEntry,
    CsiParam,
    OscString,
    EscSwallow,  // consume one byte after ESC) ESC#
    Charset,     // the byte after ESC(: which set G0 holds
  };

  State _state = State::Ground;

  static constexpr int MAX_PARAMS = 16;
  int _params[MAX_PARAMS] = {};
  int _paramCount = 0;
  bool _questionMark = false;   // for DEC private modes (ESC [ ?)
  bool _hasIntermediate = false; // CSI sequence has intermediate bytes (0x20-0x2F)
  char _csiPrefix = 0;          // CSI parameter prefix: '>', '=', '<', etc.

  void handleGround(uint8_t byte);
  void handleEscape(uint8_t byte);
  void handleCsiEntry(uint8_t byte);
  void handleCsiParam(uint8_t byte);
  void dispatchCsi(uint8_t cmd);
  void handleSgr();
  void print(uint32_t cp);

  int param(int idx, int def = 0) const;
  void resetParams();

  bool _cursorVisible = true;
  bool _appCursor = false;
  bool _lineDrawing = false;  // G0 is DEC Special Graphics
  uint32_t _last = 0;         // the last character printed, for REP
  void (*_reply)(const char*) = nullptr;
  void reply(const char* s) { if (_reply) _reply(s); }

  // UTF-8 decoder state
  uint32_t _utf8Cp = 0;
  int _utf8Remaining = 0;
};
