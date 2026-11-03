#pragma once

// Display dimensions (native landscape)
#define DISPLAY_W 800
#define DISPLAY_H 480

// Font cell size
#define TERM_FONT_W 10
#define TERM_FONT_H 20

// Margins so text stays clear of the bezel: 20 px left and right, 12 px on top, 8 px between the
// terminal and the status bar, which takes the bottom 20 px.
#define TERM_OFFSET_X 20
#define TERM_OFFSET_Y 12
#define STATUS_Y (DISPLAY_H - TERM_FONT_H)

#define TERM_COLS ((DISPLAY_W - TERM_OFFSET_X * 2) / TERM_FONT_W)  // 76
#define TERM_ROWS ((STATUS_Y - 8 - TERM_OFFSET_Y) / TERM_FONT_H)   // 22

// Display refresh thresholds
#define FULL_REFRESH_INTERVAL   20      // Full refresh every N fast refreshes
#define MIN_REFRESH_INTERVAL_MS 300     // Minimum ms between display refreshes

// Serial
#define TERM_BAUD 115200

// Tab width
#define TAB_WIDTH 8
