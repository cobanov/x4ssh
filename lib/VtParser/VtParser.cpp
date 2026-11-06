#include "VtParser.h"
#include <cstdio>

// DEC Special Graphics, 0x60-0x7E: what ncurses draws boxes with on an xterm
static const uint16_t kLineDrawing[] = {
    0x25C6, 0x2592, 0x2409, 0x240C, 0x240D, 0x240A, 0x00B0, 0x00B1, 0x2424, 0x240B, 0x2518, 0x2510, 0x250C, 0x2514, 0x253C, 0x23BA,
    0x23BB, 0x2500, 0x23BC, 0x23BD, 0x251C, 0x2524, 0x2534, 0x252C, 0x2502, 0x2264, 0x2265, 0x03C0, 0x2260, 0x00A3, 0x00B7,
};

// Width as the host's wcwidth counts it, or everything after the character sits in the wrong column.
// shortcut: the emoji blocks are taken whole and the combining marks are the common ones; widen these if a prompt drifts.
static const struct Range {
  uint32_t lo, hi;
} kZeroWidth[] = {
    {0x0300, 0x036F}, {0x200B, 0x200F}, {0x2060, 0x2064}, {0x20D0, 0x20F0}, {0xFE00, 0xFE0F}, {0xFE20, 0xFE2F}, {0xFEFF, 0xFEFF}, {0xE0100, 0xE01EF},
}, kWide[] = {
    {0x1100, 0x115F}, {0x231A, 0x231B}, {0x2329, 0x232A}, {0x23E9, 0x23EC}, {0x23F0, 0x23F0}, {0x23F3, 0x23F3}, {0x25FD, 0x25FE}, {0x2614, 0x2615},
    {0x2648, 0x2653}, {0x267F, 0x267F}, {0x2693, 0x2693}, {0x26A1, 0x26A1}, {0x26AA, 0x26AB}, {0x26BD, 0x26BE}, {0x26C4, 0x26C5}, {0x26CE, 0x26CE},
    {0x26D4, 0x26D4}, {0x26EA, 0x26EA}, {0x26F2, 0x26F3}, {0x26F5, 0x26F5}, {0x26FA, 0x26FA}, {0x26FD, 0x26FD}, {0x2705, 0x2705}, {0x270A, 0x270B},
    {0x2728, 0x2728}, {0x274C, 0x274C}, {0x274E, 0x274E}, {0x2753, 0x2755}, {0x2757, 0x2757}, {0x2795, 0x2797}, {0x27B0, 0x27B0}, {0x27BF, 0x27BF},
    {0x2B1B, 0x2B1C}, {0x2B50, 0x2B50}, {0x2B55, 0x2B55}, {0x2E80, 0x303E}, {0x3041, 0x4DBF}, {0x4E00, 0xA4CF}, {0xA960, 0xA97F}, {0xAC00, 0xD7A3},
    {0xF900, 0xFAFF}, {0xFE10, 0xFE19}, {0xFE30, 0xFE6F}, {0xFF01, 0xFF60}, {0xFFE0, 0xFFE6}, {0x1F004, 0x1F004}, {0x1F0CF, 0x1F0CF}, {0x1F18E, 0x1F18E},
    {0x1F191, 0x1F19A}, {0x1F200, 0x1F265}, {0x1F300, 0x1F64F}, {0x1F680, 0x1F6FF}, {0x1F900, 0x1F9FF}, {0x1FA70, 0x1FAFF}, {0x20000, 0x3FFFD},
};

template <size_t N>
static bool inRanges(const Range (&ranges)[N], uint32_t cp) {
  for (const Range& r : ranges) {
    if (cp >= r.lo && cp <= r.hi) return true;
  }
  return false;
}

void VtParser::print(uint32_t cp) {
  if (cp >= 0x300 && inRanges(kZeroWidth, cp)) return;
  const bool wide = cp >= 0x1100 && inRanges(kWide, cp);
  _last = cp;
  if (wide && _buf.cursorCol() == TERM_COLS - 1 && !_buf.wrapPending()) _buf.putChar(' ');  // both halves on one line, as on xterm
  _buf.putChar(cp > 0xFFFF ? '?' : (uint16_t)cp);  // a cell holds 16 bits
  if (wide) _buf.putChar(' ');
}

void VtParser::resetParams() {
  for (int i = 0; i < MAX_PARAMS; i++) _params[i] = 0;
  _paramCount = 0;
  _questionMark = false;
  _hasIntermediate = false;
  _csiPrefix = 0;
}

int VtParser::param(int idx, int def) const {
  if (idx >= _paramCount) return def;
  return _params[idx] == 0 ? def : _params[idx];
}

void VtParser::feed(uint8_t byte) {
  // ESC starts over from anywhere, so a sequence cut in half does not eat the next one
  if (byte == 0x1B && _state != State::Ground) {
    _state = State::Escape;
    return;
  }
  switch (_state) {
    case State::Ground:    handleGround(byte);  break;
    case State::Escape:    handleEscape(byte);   break;
    case State::CsiEntry:  handleCsiEntry(byte); break;
    case State::CsiParam:  handleCsiParam(byte); break;
    case State::OscString:
      // Consume until BEL or ST (ESC \): Escape then eats the backslash
      if (byte == 0x07) _state = State::Ground;
      break;
    case State::Charset:
      _lineDrawing = byte == '0';
      _state = State::Ground;
      break;
    case State::EscSwallow:
      // Swallow one byte (charset designation parameter) and return to ground
      _state = State::Ground;
      break;
  }
}

void VtParser::handleGround(uint8_t byte) {
  // UTF-8 continuation byte
  if (_utf8Remaining > 0) {
    if ((byte & 0xC0) == 0x80) {
      _utf8Cp = (_utf8Cp << 6) | (byte & 0x3F);
      _utf8Remaining--;
      if (_utf8Remaining == 0) print(_utf8Cp);
      return;
    }
    // Invalid continuation - reset and fall through
    _utf8Remaining = 0;
  }

  if (byte == 0x1B) {
    _state = State::Escape;
    return;
  }

  switch (byte) {
    case 0x07: break;                 // BEL - ignore
    case 0x08: _buf.backspace(); break;  // BS
    case 0x09: _buf.tab(); break;        // HT
    case 0x0A: // LF
    case 0x0B: // VT
    case 0x0C: // FF
      _buf.lineFeed();
      break;
    case 0x0D: _buf.carriageReturn(); break;  // CR
    default:
      if (byte >= 0x20 && byte < 0x7F) {
        print(_lineDrawing && byte >= 0x60 ? kLineDrawing[byte - 0x60] : byte);
      } else if (byte >= 0xC0 && byte < 0xE0) {
        // UTF-8 2-byte sequence start
        _utf8Cp = byte & 0x1F;
        _utf8Remaining = 1;
      } else if (byte >= 0xE0 && byte < 0xF0) {
        // UTF-8 3-byte sequence start (BMP: U+0800-U+FFFF)
        _utf8Cp = byte & 0x0F;
        _utf8Remaining = 2;
      } else if (byte >= 0xF0 && byte < 0xF8) {
        // UTF-8 4-byte sequence start (>BMP, will truncate to 16-bit)
        _utf8Cp = byte & 0x07;
        _utf8Remaining = 3;
      }
      break;
  }
}

void VtParser::handleEscape(uint8_t byte) {
  switch (byte) {
    case '[':
      _state = State::CsiEntry;
      resetParams();
      break;
    case ']':
    case 'P':  // DCS
    case 'X':  // SOS
    case '^':  // PM
    case '_':  // APC
      _state = State::OscString;
      break;
    case 'D':  // IND - index (move down, scroll if at bottom)
      _buf.lineFeed();
      _state = State::Ground;
      break;
    case 'M':  // RI - reverse index (move up, scroll if at top)
      _buf.reverseIndex();
      _state = State::Ground;
      break;
    case '7':  // DECSC - save cursor
      _buf.saveCursor();
      _state = State::Ground;
      break;
    case '8':  // DECRC - restore cursor
      _buf.restoreCursor();
      _state = State::Ground;
      break;
    case 'c':  // RIS - full reset
      _buf.resetAttrs();  // first: erasing paints the current background
      _buf.switchScreen(false);
      _buf.setScrollRegion(0, TERM_ROWS - 1);
      _buf.eraseDisplay(2);
      _buf.setCursor(0, 0);
      _cursorVisible = true;
      _appCursor = false;
      _lineDrawing = false;
      _last = 0;
      _state = State::Ground;
      break;
    case '=':  // DECKPAM - keypad application mode (ignore)
    case '>':  // DECKPNM - keypad numeric mode (ignore)
      _state = State::Ground;
      break;
    case '(':  // G0 charset designation
      _state = State::Charset;
      break;
    case ')':  // G1 charset designation - consume next byte
    case '#':  // DEC line drawing - consume next byte
    case '*':  // G2 charset designation - consume next byte
    case '+':  // G3 charset designation - consume next byte
      _state = State::EscSwallow;
      break;
    default:
      _state = State::Ground;
      break;
  }
}

void VtParser::handleCsiEntry(uint8_t byte) {
  if (byte == '?') {
    _questionMark = true;
    _state = State::CsiParam;
    return;
  }
  // Handle other parameter prefix bytes (>, =, <)
  if (byte == '>' || byte == '=' || byte == '<') {
    _csiPrefix = byte;
    _state = State::CsiParam;
    return;
  }
  // Fall through to param handling
  _state = State::CsiParam;
  handleCsiParam(byte);
}

void VtParser::handleCsiParam(uint8_t byte) {
  if (byte >= '0' && byte <= '9') {
    if (_paramCount == 0) _paramCount = 1;
    // Stops well past the screen and well short of overflowing: the host picks these digits
    int& p = _params[_paramCount - 1];
    if (p < 10000) p = p * 10 + (byte - '0');
    return;
  }
  if (byte == ';') {
    if (_paramCount == 0) _paramCount = 1;  // the first parameter was left empty
    if (_paramCount < MAX_PARAMS) _paramCount++;
    return;
  }
  // Intermediate bytes (0x20-0x2F: space ! " # $ % & ' ( ) * + , - . /) and colon sub-parameters
  // Collect but mark as having intermediates so we can ignore unsupported sequences
  if ((byte >= 0x20 && byte <= 0x2F) || (byte >= 0x3A && byte <= 0x3F)) {
    _hasIntermediate = true;
    return;
  }
  // Dispatch final byte (0x40-0x7E)
  if (byte >= 0x40 && byte <= 0x7E) {
    // Dispatch if no intermediates and no unrecognized prefix
    // _questionMark sequences go through dispatchCsi which handles them
    if (!_hasIntermediate && _csiPrefix == 0) {
      dispatchCsi(byte);
    }
    // Sequences with intermediates or prefixes (like ESC[>c) are
    // silently consumed — the final byte ends the sequence cleanly
    _state = State::Ground;
    return;
  }
  // Unknown byte - abort sequence cleanly
  _state = State::Ground;
}

void VtParser::dispatchCsi(uint8_t cmd) {
  int n = param(0, 1);

  if (_questionMark) {
    if (cmd != 'h' && cmd != 'l') return;
    const bool set = cmd == 'h';  // DECSET, or DECRST
    for (int i = 0; i < _paramCount; i++) {
      switch (_params[i]) {
        case 1: _appCursor = set; break;       // DECCKM
        case 25: _cursorVisible = set; break;  // DECTCEM
        case 47:    // alt screen (simple)
        case 1047:  // alt screen
        case 1049:  // switchScreen keeps the cursor itself: DECSC's slot belongs to the program
          _buf.switchScreen(set);
          break;
      }
    }
    return;
  }

  switch (cmd) {
    case 'A': _buf.moveCursorUp(n); break;       // CUU
    case 'B': _buf.moveCursorDown(n); break;      // CUD
    case 'C': _buf.moveCursorForward(n); break;   // CUF
    case 'D': _buf.moveCursorBack(n); break;      // CUB
    case 'E':  // CNL - cursor next line
      _buf.moveCursorDown(n);
      _buf.carriageReturn();
      break;
    case 'F':  // CPL - cursor previous line
      _buf.moveCursorUp(n);
      _buf.carriageReturn();
      break;
    case 'G':  // CHA - cursor horizontal absolute
      _buf.setCursor(_buf.cursorRow(), param(0, 1) - 1);
      break;
    case 'H':  // CUP - cursor position
    case 'f':  // HVP - same as CUP
      _buf.setCursor(param(0, 1) - 1, param(1, 1) - 1);
      break;
    case 'J': _buf.eraseDisplay(param(0, 0)); break;  // ED
    case 'K': _buf.eraseLine(param(0, 0)); break;      // EL
    case 'L': _buf.insertLines(n); break;   // IL
    case 'M': _buf.deleteLines(n); break;   // DL
    case 'P': _buf.deleteChars(n); break;   // DCH
    case '@': _buf.insertChars(n); break;   // ICH
    case 'S': _buf.scrollUp(n); break;      // SU
    case 'T': _buf.scrollDown(n); break;    // SD
    case 'd':  // VPA - vertical position absolute
      _buf.setCursor(param(0, 1) - 1, _buf.cursorCol());
      break;
    case 'm': handleSgr(); break;            // SGR
    case 'r':  // DECSTBM - set scroll region
      _buf.setScrollRegion(param(0, 1) - 1, param(1, TERM_ROWS) - 1);
      break;
    case 'n':  // DSR - device status report
      if (param(0, 0) == 6) {
        // Cursor position report: ESC [ row ; col R (1-based)
        char resp[20];
        snprintf(resp, sizeof(resp), "\033[%d;%dR",
                 _buf.cursorRow() + 1, _buf.cursorCol() + 1);
        reply(resp);
      } else if (param(0, 0) == 5) {
        reply("\033[0n");  // status: OK
      }
      break;
    case 's': _buf.saveCursor(); break;     // ANSI save cursor
    case 'u': _buf.restoreCursor(); break;  // ANSI restore cursor
    case 'X': _buf.eraseChars(n); break;    // ECH - erase characters
    case 'Z': {                             // CBT - back n tab stops
      int col = _buf.cursorCol();
      for (int i = 0; i < n && col > 0; i++) col = (col - 1) / TAB_WIDTH * TAB_WIDTH;
      _buf.setCursor(_buf.cursorRow(), col);
      break;
    }
    case 'b':  // REP - ncurses draws rules with it; past a full screen more changes nothing
      for (int i = 0; _last && i < n && i < TERM_ROWS * TERM_COLS; i++) print(_last);
      break;
    case 'c':  // DA - device attributes
      reply("\033[?1;0c");  // VT100 with no options
      break;
  }
}

// Standard ANSI color palette: approximate luminance (0-255) for colors 0-15
static const uint8_t kAnsiLum[16] = {
    0,  // 0: black
   76,  // 1: red
  149,  // 2: green
  226,  // 3: yellow
   29,  // 4: blue
  105,  // 5: magenta
  178,  // 6: cyan
  200,  // 7: white (light gray)
  128,  // 8: bright black (dark gray)
  128,  // 9: bright red
  192,  // 10: bright green
  255,  // 11: bright yellow
   80,  // 12: bright blue
  160,  // 13: bright magenta
  224,  // 14: bright cyan
  255,  // 15: bright white
};

// Compute luminance from RGB
static uint8_t lumRGB(int r, int g, int b) {
  return (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
}

// Compute luminance from 256-color palette index
static uint8_t lum256(int n) {
  if (n < 16) return kAnsiLum[n];
  if (n >= 232) {
    // Grayscale ramp: 232=dark(8), 255=light(238)
    return 8 + (n - 232) * 10;
  }
  // 6x6x6 color cube (indices 16-231), each axis 0,95,135,175,215,255
  auto level = [](int v) { return v ? v * 40 + 55 : 0; };
  const int idx = n - 16;
  return lumRGB(level(idx / 36), level(idx / 6 % 6), level(idx % 6));
}

void VtParser::handleSgr() {
  if (_paramCount == 0) {
    _buf.resetAttrs();
    return;
  }

  // Foreground colors have nothing to show on a one-ink panel, so 30-37, 39 and 90-97 do nothing.
  for (int i = 0; i < _paramCount; i++) {
    const int p = _params[i];
    if (p >= 40 && p <= 47) {
      _buf.setBgBright(kAnsiLum[p - 40]);
    } else if (p >= 100 && p <= 107) {
      _buf.setBgBright(kAnsiLum[p - 100 + 8]);
    }
    switch (p) {
      case 0:  _buf.resetAttrs(); break;
      case 1:  _buf.setAttr(TermCell::ATTR_BOLD); break;
      case 2:  _buf.clearAttr(TermCell::ATTR_BOLD); break;  // dim
      case 4:  _buf.setAttr(TermCell::ATTR_UNDERLINE); break;
      case 7:  _buf.setAttr(TermCell::ATTR_INVERSE); break;
      case 22: _buf.clearAttr(TermCell::ATTR_BOLD); break;
      case 24: _buf.clearAttr(TermCell::ATTR_UNDERLINE); break;
      case 27: _buf.clearAttr(TermCell::ATTR_INVERSE); break;
      case 49: _buf.setBgBright(255); break;  // default bg (white)

      // Extended color, 5;N or 2;R;G;B: the arguments are skipped either way
      case 38:
      case 48: {
        const int kind = i + 1 < _paramCount ? _params[i + 1] : 0;
        if (kind == 5) {
          if (p == 48 && i + 2 < _paramCount) _buf.setBgBright(lum256(_params[i + 2]));
          i += 2;
        } else if (kind == 2) {
          if (p == 48 && i + 4 < _paramCount) _buf.setBgBright(lumRGB(_params[i + 2], _params[i + 3], _params[i + 4]));
          i += 4;
        }
        break;
      }
    }
  }
}
