// The site hero's film: one scripted boot, session and sleep, drawn through the real renderer, one frame per
// panel refresh. film OUT_DIR writes NNN.pbm and index.txt ("NNN kind hold key mark"). site/film.sh runs it.
#include <cstdio>
#include <cstring>
#include <string>

#include "EInkDisplay.h"
#include "TermBuffer.h"
#include "TermRenderer.h"
#include "mascot.h"
#include "status.h"

static EInkDisplay display;
static TermBuffer buf;
static TermRenderer renderer(display, buf);
static std::string dir;
static FILE* index_;
static int n = 0;

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
static void status(const char* left, const char* right) {
  uint16_t cells[TERM_COLS];
  textStatus(cells, TERM_COLS, left, right);
  renderer.setStatus(cells);
}
// key: the keycap shown as this frame lands, or -. mark: a beat's name, or -.
static void shot(const char* kind, int hold, const char* key = "-", const char* mark = "-") {
  if (!strcmp(kind, "full")) renderer.renderFull();
  else renderer.renderDirty();
  char name[16];
  snprintf(name, sizeof name, "%03d", n);
  FILE* f = fopen((dir + "/" + name + ".pbm").c_str(), "wb");
  fprintf(f, "P4\n800 480\n");
  for (size_t i = 0; i < sizeof display.fb; i++) fputc(display.fb[i] ^ 0xFF, f);
  fclose(f);
  fprintf(index_, "%s %s %d %s %s\n", name, kind, hold, key, mark);
  n++;
}
static void sleepScreen() {
  buf.eraseDisplay(2);
  at(MASCOT_LAST_ROW + 1, "press power to wake");
  at(MASCOT_LAST_ROW + 2, "x.com/mertcobanov");
  status("sleeping since 23:58", "99%");
  renderer.setArt(kMascotAsleep);
  renderer.setCursorVisible(false);
}
// The panel takes keys in bursts: each refresh catches what was typed since the last.
static void burst(std::initializer_list<const char*> parts, const char* mark = "-") {
  for (const char* part : parts) {
    type(part);
    shot("fast", 430, "-", mark);
    mark = "-";
  }
}

int main(int, char** argv) {
  dir = argv[1];
  index_ = fopen((dir + "/index.txt").c_str(), "w");

  sleepScreen();
  shot("full", 1200, "-", "asleep");

  buf.eraseDisplay(2);
  at(MASCOT_LAST_ROW + 1, "XTEINK SSH");
  at(MASCOT_LAST_ROW + 2, "x.com/mertcobanov");
  buf.setCursor(MASCOT_LAST_ROW + 4, 0);
  status("wifi: --", "99% | --:--");
  renderer.setArt(kMascotAwake);
  renderer.setCursorVisible(true);
  shot("full", 700, "power", "boot");
  type("type help\nx4> ");
  const uint8_t* const sweep[] = {kMascotScan1, kMascotScan2, kMascotScan3};
  for (int i = 0; i < 3; i++) {
    renderer.setArt(sweep[i % 3]);
    shot("fast", 1000, "-", i ? "-" : "scan");
  }
  renderer.setArt(kMascotAwake);
  status("wifi: homelab 72%", "99% | 23:58");
  shot("fast", 1000, "-", "online");

  burst({"ssh", " cob", "anov", "@nuc"}, "typing");
  type("\nconnecting to nuc:22\n");
  shot("fast", 900, "enter");

  // shell(): the art comes down and the host's login text scrolls the splash away.
  renderer.setArt(nullptr);
  type("Welcome to Ubuntu 24.04.3 LTS\n\n"
       "  System load:  0.08    Processes:  214\n"
       "  Usage of /:   41.2%   Users:      1\n"
       "  Memory usage: 23%     IPv4:       192.168.1.10\n"
       "  Swap usage:   0%\n\n"
       "0 updates can be applied immediately.\n\n"
       "Last login: Mon Nov  2 21:14:07 2026 from laptop\n"
       "cobanov@nuc:~$ ");
  shot("fast", 700, "-", "session");
  burst({"upt", "ime ", "-p"});
  type("\nup 6 weeks, 3 hours, 17 minutes\ncobanov@nuc:~$ ");
  shot("fast", 800, "enter");
  burst({"docke", "r ps ", "--for", "mat {", "{.Nam", "es}}"});
  type("\nwatchdock\ngrafana\nprometheus\ncaddy\nntfy\nterminal-army\ncobanov@nuc:~$ ");
  shot("fast", 2400, "enter", "output");

  sleepScreen();
  shot("full", 300, "power", "sleep");
  fclose(index_);
}
