#pragma once
#include <BleKeyboardHost.h>

// A key as the bytes an xterm sends; `appCursor` is DECCKM (arrows, Home, End as ESC O x). Writes at most 5 bytes.
inline size_t keyBytes(const freeink::KeyEvent& e, uint8_t* out, bool appCursor = false) {
  using K = freeink::SpecialKey;
  static const struct {
    K key;
    const char* seq;
  } kSpecial[] = {
      {K::Enter, "\r"},      {K::Backspace, "\x7f"}, {K::Tab, "\t"},         {K::Escape, "\033"},
      {K::Delete, "\033[3~"}, {K::Up, "\033[A"},      {K::Down, "\033[B"},     {K::Right, "\033[C"},
      {K::Left, "\033[D"},    {K::Home, "\033[H"},    {K::End, "\033[F"},      {K::PageUp, "\033[5~"},
      {K::PageDown, "\033[6~"},
  };
  const bool ctrl = e.mods & 0x11, alt = e.mods & 0x44, gui = e.mods & 0x88;
  if (gui) return 0;  // Cmd/Win shortcuts belong to no one here
  size_t n = 0;
  if (alt) out[n++] = 0x1b;  // Meta as an ESC prefix, like xterm's metaSendsEscape
  if (e.ch) {
    char c = e.ch;
    if (ctrl && c >= '@' && c <= '~') c &= 0x1f;  // Ctrl-A..Z, Ctrl-[ = ESC, Ctrl-_ ...
    else if (ctrl && (c == ' ' || c == '2')) c = 0;
    else if (ctrl && c == '/') c = 0x1f;
    out[n++] = c;
    return n;
  }
  for (auto& s : kSpecial) {
    if (s.key != e.special) continue;
    for (const char* p = s.seq; *p; p++) out[n++] = *p;
    if (appCursor && n >= 3 && out[n - 2] == '[' && out[n - 1] >= 'A' && out[n - 1] <= 'H') out[n - 2] = 'O';
    return n;
  }
  return 0;
}
