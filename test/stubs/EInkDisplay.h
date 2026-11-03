// Host stand-in for the panel: the framebuffer, and a count of the refreshes asked of it.
#pragma once
#include <cstdint>
#include <cstring>
class EInkDisplay {
 public:
  enum RefreshMode { FULL_REFRESH, FAST_REFRESH };
  uint8_t* getFrameBuffer() { return fb; }
  void displayBuffer(RefreshMode) { refreshes++; }
  uint8_t fb[800 / 8 * 480];
  int refreshes = 0;
  EInkDisplay() { memset(fb, 0xFF, sizeof fb); }
};
