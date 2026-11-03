#pragma once
// Bottom status bar: " wifi: homelab 72%" on the left, "99% | 23:15 " on the right, one codepoint per cell.
#include <cstdint>
#include <cstdio>

// Signal quality from RSSI: -100 dBm = 0 %, -50 dBm and better = 100 %.
inline int wifiPercent(int rssi) {
  const int q = 2 * (rssi + 100);
  return q < 0 ? 0 : q > 100 ? 100 : q;
}

// `left` one cell in from the left edge, `right` one cell in from the right; the right side wins.
inline void textStatus(uint16_t* out, int cols, const char* left, const char* right) {
  for (int i = 0; i < cols; i++) out[i] = ' ';
  int r = 0;
  while (right[r]) r++;
  const int rightAt = cols - 1 - r;
  for (int i = 0; i < r; i++) out[rightAt + i] = (uint8_t)right[i];
  for (int i = 0, c = 1; left[i] && c < rightAt - 1; i++, c++) out[c] = (uint8_t)left[i];
}

// ssid null = not connected; hour < 0 = clock not synced yet.
inline void buildStatus(uint16_t* out, int cols, const char* ssid, int rssi, int battery, int hour, int minute) {
  char left[48], right[24];
  if (hour >= 0) snprintf(right, sizeof right, "%d%% | %02d:%02d", battery, hour % 100, minute % 100);
  else snprintf(right, sizeof right, "%d%% | --:--", battery);
  if (ssid) snprintf(left, sizeof left, "wifi: %.24s %d%%", ssid, wifiPercent(rssi));
  else snprintf(left, sizeof left, "wifi: --");
  textStatus(out, cols, left, right);
}
