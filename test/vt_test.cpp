// Bytes a remote host can send, through the real parser and buffer. run.sh builds this with ASan and UBSan,
// so a sequence that writes outside the screen fails here and not on the device.
#include <cassert>
#include <cstdio>
#include <string>

#include "VtParser.h"

static TermBuffer buf;
static VtParser vt(buf);

static void feed(const std::string& s) {
  for (unsigned char c : s) vt.feed(c);
}
static std::string row(int r, int n) {
  std::string s;
  for (int c = 0; c < n; c++) s += (char)buf.cellAt(r, c).codepoint;
  return s;
}

int main() {
  // Counts past the screen and past what an int holds, for every command that takes one
  for (const char* cmd : {"P", "@", "X", "L", "M", "S", "T", "A", "B", "C", "D", "E", "F", "G", "H", "d", "m", "r"}) {
    for (const char* n : {"1000", "99999", "2147483648", "4294967295", "4294967291", "99999999999999999999"}) {
      for (const char* at : {"\033[1;1H", "\033[9;40H", "\033[22;76H"}) {
        feed("\033c");
        feed(at);
        feed(std::string("\033[") + n + cmd);
        feed("x");
        assert(buf.cursorRow() >= 0 && buf.cursorRow() < TERM_ROWS);
        assert(buf.cursorCol() >= 0 && buf.cursorCol() < TERM_COLS);
      }
    }
  }
  feed("\033[48;5;4294967295m\033[48;2;4294967295;1;1mx\033[0m");

  // Delete closes the gap, whatever the count
  feed("\033cabcdef\033[1;2H\033[2P");
  assert(row(0, 4) == "adef");
  feed("\033cabc\033[1;2H\033[1000P");
  assert(buf.cellAt(0, 0).codepoint == 'a' && buf.cellAt(0, 1).codepoint != 'b' && buf.cellAt(0, 2).codepoint != 'c');

  // An empty first parameter is still the first one
  feed("\033c\033[;5Hx");
  assert(buf.cellAt(0, 4).codepoint == 'x');

  // Strings end at ST and leave nothing behind
  feed("\033c\033]0;title\033\\x");
  assert(row(0, 2).substr(0, 1) == "x" && buf.cellAt(0, 1).codepoint != '\\');
  feed("\033c\033P+q544e\033\\y");
  assert(buf.cellAt(0, 0).codepoint == 'y');
  feed("\033c\033_note\033\\z");
  assert(buf.cellAt(0, 0).codepoint == 'z');

  // Colon sub-parameters are dropped whole, not printed
  feed("\033c\033[38:2::255:0:0mq");
  assert(buf.cellAt(0, 0).codepoint == 'q');

  // A wrap left pending does not outlive a scroll region or a restored cursor
  feed("\033c\033[1;76Hx\033[2;20r");
  feed("y");
  assert(buf.cellAt(0, 0).codepoint == 'y');
  puts("vt ok");
}
