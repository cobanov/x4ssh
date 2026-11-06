// Draws through the real renderer into a host framebuffer. With an argument, also writes the screens
// the design kit shows: test/art_test OUT_DIR gives boot, scan, sleep and terminal as PBM.
#include <cassert>
#include <cstdio>
#include <string>

#include "EInkDisplay.h"
#include "TermBuffer.h"
#include "TermRenderer.h"
#include "mascot.h"
#include "status.h"

static EInkDisplay display;
static TermBuffer buf;
static TermRenderer renderer(display, buf);

static void type(const char* s) {
  for (; *s; s++) {
    if (*s == '\n') { buf.carriageReturn(); buf.lineFeed(); }
    else buf.putChar((uint8_t)*s);
  }
}
static void at(int row, const char* s) {
  buf.setCursor(row, (TERM_COLS - (int)strlen(s)) / 2);
  type(s);
}
static bool black(int x, int y) { return !(display.fb[y * 100 + x / 8] >> (7 - x % 8) & 1); }
static int ink(int x0, int y0, int x1, int y1) {
  int n = 0;
  for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) n += black(x, y);
  return n;
}
static void dump(const std::string& dir, const char* name) {
  if (dir.empty()) return;
  FILE* f = fopen((dir + "/" + name + ".pbm").c_str(), "wb");
  fprintf(f, "P4\n800 480\n");
  for (size_t i = 0; i < sizeof display.fb; i++) fputc(display.fb[i] ^ 0xFF, f);
  fclose(f);
}
static void status(const char* left, const char* right) {
  uint16_t cells[TERM_COLS];
  textStatus(cells, TERM_COLS, left, right);
  renderer.setStatus(cells);
}

// The status bar's middle is black except for the snail.
static int icon() { return 25 * 18 - ink(387, 461, 412, 479); }

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : "";
  static_assert(MASCOT_Y >= TERM_OFFSET_Y + MASCOT_FIRST_ROW * TERM_FONT_H);
  static_assert(MASCOT_Y + MASCOT_H * MASCOT_SCALE <= TERM_OFFSET_Y + (MASCOT_LAST_ROW + 1) * TERM_FONT_H);
  static_assert(MASCOT_X >= TERM_OFFSET_X && MASCOT_X + MASCOT_W * MASCOT_SCALE <= DISPLAY_W - TERM_OFFSET_X);
  static_assert(MASCOT_LAST_ROW + 5 < TERM_ROWS, "two splash lines, a gap and a prompt fit under the art");
  static_assert(sizeof kMascotAwake == (MASCOT_W + 7) / 8 * MASCOT_H);

  status("wifi: --", "99% | --:--");
  at(MASCOT_LAST_ROW + 1, "XTEINK SSH");
  at(MASCOT_LAST_ROW + 2, "x.com/mertcobanov");
  buf.setCursor(MASCOT_LAST_ROW + 4, 0);
  type("type help\nx4> ");

  // Art up: drawn, and no snail in the bar.
  renderer.setArt(kMascotScan3);
  renderer.renderDirty();
  const int scan = ink(MASCOT_X, MASCOT_Y, MASCOT_X + MASCOT_W * 3, MASCOT_Y + MASCOT_H * 3);
  assert(scan > 3000);
  assert(icon() == 0);
  dump(dir, "scan");

  // A shorter sweep leaves no trace of the longer one.
  renderer.setArt(kMascotAwake);
  renderer.renderDirty();
  const int awake = ink(MASCOT_X, MASCOT_Y, MASCOT_X + MASCOT_W * 3, MASCOT_Y + MASCOT_H * 3);
  assert(awake > 3000 && awake < scan);
  status("wifi: homelab 72%", "99% | 23:58");
  renderer.renderDirty();
  dump(dir, "boot");

  // A redraw keeps it.
  renderer.renderFull();
  assert(ink(MASCOT_X, MASCOT_Y, MASCOT_X + MASCOT_W * 3, MASCOT_Y + MASCOT_H * 3) == awake);

  // Output below it leaves it alone; text scrolling up into its rows takes it down, snail to the bar.
  type("stat\n");
  renderer.renderDirty();
  assert(renderer.hasArt());
  type("keyboard: M7\nwifi: homelab 192.168.1.40\nheap: 53120 free\nbattery: 99%\nx4> ");
  renderer.renderDirty();
  assert(!renderer.hasArt());
  assert(icon() > 50);
  dump(dir, "terminal");

  // A long network name reaches the middle of the bar: the name wins.
  status("wifi: a-network-with-a-very-long-name-indeed-yes 72%", "99% | 23:58");
  renderer.renderDirty();
  const int withName = icon();
  renderer.setArt(kMascotAwake);  // art up means no snail in the bar, so this is the name alone
  renderer.renderDirty();
  assert(icon() == withName);
  renderer.setArt(nullptr);
  status("wifi: homelab 72%", "99% | 23:58");

  // The info page: text arriving under it stays undrawn, and renderFull brings the terminal back.
  buf.eraseDisplay(2);
  renderer.setArt(nullptr);
  renderer.renderFull();
  status("wifi: homelab 100%", "99% | 23:58");
  renderer.pageBegin();
  renderer.pageLine(0, "x4ssh", true);
  renderer.pageLine(2, "buttons");
  renderer.pageEnd();
  assert(renderer.paged() && black(TERM_OFFSET_X + 700, TERM_OFFSET_Y + 2) && !black(TERM_OFFSET_X + 700, TERM_OFFSET_Y + 2 * TERM_FONT_H + 2));
  const int shown = ink(0, 0, 800, 480), drawn = display.refreshes;
  buf.setCursor(10, 0);
  type("under the page");
  renderer.renderDirty();
  assert(ink(0, 0, 800, 480) == shown && display.refreshes == drawn);
  renderer.renderFull();
  assert(!renderer.paged() && !black(TERM_OFFSET_X + 700, TERM_OFFSET_Y + 2) && ink(TERM_OFFSET_X, TERM_OFFSET_Y + 10 * TERM_FONT_H, 200, TERM_OFFSET_Y + 11 * TERM_FONT_H) > 0);

  // The cursor moves with no text changing, and the panel follows; hidden, it costs no refresh.
  buf.eraseDisplay(2);
  buf.setCursor(3, 3);
  renderer.renderFull();
  assert(!renderer.cursorMoved());
  const auto cell = [](int row, int col) {
    return ink(TERM_OFFSET_X + col * TERM_FONT_W, TERM_OFFSET_Y + row * TERM_FONT_H, TERM_OFFSET_X + (col + 1) * TERM_FONT_W, TERM_OFFSET_Y + (row + 1) * TERM_FONT_H);
  };
  assert(cell(3, 3) == TERM_FONT_W * TERM_FONT_H && cell(3, 2) == 0);
  buf.moveCursorBack(1);
  assert(buf.dirtyRows() == 0 && renderer.cursorMoved());
  renderer.renderDirty();
  assert(!renderer.cursorMoved());
  assert(cell(3, 2) == TERM_FONT_W * TERM_FONT_H && cell(3, 3) == 0);
  renderer.setCursorVisible(false);
  assert(renderer.cursorMoved());
  renderer.renderDirty();
  assert(cell(3, 2) == 0);
  buf.moveCursorForward(5);
  assert(!renderer.cursorMoved());
  renderer.setCursorVisible(true);

  // Bold strikes twice and underline runs the whole cell, on the row '_' sits on.
  buf.setCursor(0, 0);
  buf.putChar('H');
  buf.setAttr(TermCell::ATTR_BOLD);
  buf.putChar('H');
  buf.clearAttr(TermCell::ATTR_BOLD);
  buf.setAttr(TermCell::ATTR_UNDERLINE);
  buf.putChar('H');
  buf.resetAttrs();
  buf.setCursor(5, 0);
  renderer.renderDirty();
  assert(cell(0, 1) > cell(0, 0));
  assert(cell(0, 2) == cell(0, 0) + TERM_FONT_W);
  assert(ink(TERM_OFFSET_X + 2 * TERM_FONT_W, TERM_OFFSET_Y + TERM_FONT_H - 1, TERM_OFFSET_X + 3 * TERM_FONT_W, TERM_OFFSET_Y + TERM_FONT_H) == TERM_FONT_W);

  // Sleep: everything cleared, snail asleep.
  buf.eraseDisplay(2);
  at(MASCOT_LAST_ROW + 1, "press power to wake");
  at(MASCOT_LAST_ROW + 2, "x.com/mertcobanov");
  status("sleeping since 23:58", "99%");
  renderer.setArt(kMascotAsleep);
  renderer.setCursorVisible(false);
  renderer.renderFull();
  assert(renderer.hasArt());
  dump(dir, "sleep");
  puts("art ok");
}
