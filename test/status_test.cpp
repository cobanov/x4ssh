// Host test for the status bar layout. Run with test/run.sh
#include <cassert>
#include <cstdio>
#include <string>
#include "status.h"

static std::string text(const uint16_t* s, int n) {
  std::string r;
  for (int i = 0; i < n; i++) r += (char)s[i];
  return r;
}

int main() {
  const int W = 76;
  uint16_t s[W];
  buildStatus(s, W, "homelab", -64, 99, 9, 5);
  std::string a = text(s, W);
  assert(a.rfind(" wifi: homelab 72% ", 0) == 0);
  assert(a.substr(W - 12) == "99% | 09:05 ");
  buildStatus(s, W, nullptr, 0, 100, -1, 0);
  a = text(s, W);
  assert(a.rfind(" wifi: -- ", 0) == 0 && a.substr(W - 13) == "100% | --:-- ");
  buildStatus(s, 30, "a-very-long-network-name-that-goes-on", -40, 5, 23, 59);
  a = text(s, 30);
  assert(a.substr(30 - 11) == "5% | 23:59 " && a.find("wifi: a-very") == 1);  // the right side wins
  assert(wifiPercent(-40) == 100 && wifiPercent(-75) == 50 && wifiPercent(-120) == 0);
  puts("status ok");
}
