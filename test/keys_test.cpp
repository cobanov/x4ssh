// Host test for keyBytes(): run with test/run.sh
#include <cassert>
#include <cstdio>
#include <string>
#include "keys.h"

unsigned long millis() { return 0; }

static std::string bytes(char ch, uint8_t mods, freeink::SpecialKey sp = freeink::SpecialKey::None, bool app = false) {
  freeink::KeyEvent e;
  e.ch = ch;
  e.mods = mods;
  e.special = sp;
  uint8_t out[8];
  size_t n = keyBytes(e, out, app);
  return std::string((char*)out, n);
}

int main() {
  using K = freeink::SpecialKey;
  assert(bytes('a', 0) == "a");
  assert(bytes('A', 0x02) == "A");
  assert(bytes('c', 0x01) == "\x03");        // left ctrl
  assert(bytes('C', 0x12) == "\x03");        // right ctrl + shift
  assert(bytes('[', 0x01) == "\x1b");
  assert(bytes(' ', 0x01) == std::string(1, '\0'));
  assert(bytes('b', 0x04) == "\x1b" "b");    // alt = meta
  assert(bytes('v', 0x08).empty());          // cmd shortcuts are dropped
  assert(bytes(0, 0, K::Up) == "\033[A");
  assert(bytes(0, 0, K::Backspace) == "\x7f");
  assert(bytes(0, 0x04, K::Left) == "\033\033[D");
  assert(bytes(0, 0, K::PageDown) == "\033[6~");
  assert(bytes(0, 0, K::None).empty());
  assert(bytes(0, 0, K::Up, true) == "\033OA");        // DECCKM
  assert(bytes(0, 0, K::Home, true) == "\033OH");
  assert(bytes(0, 0x04, K::Left, true) == "\033\033OD");
  assert(bytes(0, 0, K::PageUp, true) == "\033[5~");   // not a cursor key
  assert(bytes('a', 0, K::None, true) == "a");

  BackButton back;
  assert(back.update(true, false, 100) == BackButton::None);   // held, not long enough yet
  assert(back.update(false, true, 100) == BackButton::Esc);    // a tap: Esc on release
  assert(back.update(true, false, 900) == BackButton::Info);   // a hold: the page, once
  assert(back.update(true, false, 950) == BackButton::None);
  assert(back.update(false, true, 1000) == BackButton::None);  // and no Esc after it
  back.spent = true;                                           // Confirm + Back redrew
  assert(back.update(false, true, 100) == BackButton::None);
  assert(back.update(false, true, 100) == BackButton::Esc);    // the next tap is a tap again
  puts("keys ok");
}
