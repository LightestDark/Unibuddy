#pragma once
/*
 * ============================================================
 *  epaper.h — E-Paper driver & multi-screen renderer
 *  Waveshare 2.13" V4 (250×122, BW)
 *
 *  Screens:
 *    Pet face      – big animated eyes        (landscape)
 *    Sleep         – closed eyes + zzz        (landscape, adaptive rotation)
 *    Temp/Calendar – date, time, weather      (portrait, two directions)
 *    Pomodoro      – focused eyes + timer     (landscape flipped)
 *    Break         – break countdown          (landscape flipped)
 * ============================================================
 */

#include <SPI.h>
#include "config.h"
#include "epd2in13_V4.h"
#include "epdpaint.h"
#include "pet.h"
#include "pomodoro.h"
#include "behaviour.h"

#define B  COL_BLACK
#define W  COL_WHITE
#define COL_BLACK  0
#define COL_WHITE  1

static Epd epd;
static unsigned char _fb[128 / 8 * 250];   // 4000 bytes
static Paint paint(_fb, 128, 250);

static int  _partialCount = 0;
static const int PARTIAL_LIMIT = 30;

/* sleep zzz animation state */
static uint8_t  _sleepFrame  = 0;
static uint32_t _sleepTimer  = 0;

// ── forward declarations ────────────────────────────────────
void drawPetFace();
void drawSleepFace();
void drawTempCalPortrait();
void drawFocusScreen();
void drawBreakScreen();
void renderToBuffer(int mode);

// ═══════════════════════════════════════════════════════════
//  Init / splash / refresh
// ═══════════════════════════════════════════════════════════

void initDisplay() {
  if (epd.Init(FULL) != 0) {
    Serial.println(F("[EPD] Init failed!"));
    return;
  }
  epd.Clear();
  paint.SetRotate(ROTATE_270);
  Serial.println(F("[EPD] Ready (landscape 250x122)"));
}

void setDisplayRotation(int r) {
  paint.SetRotate(r);
}

void showSplashScreen() {
  paint.Clear(W);
  paint.DrawStringAt(40, 12, "UniBuddy", &Font24, B);
  paint.DrawHorizontalLine(30, 42, 190, B);
  paint.DrawStringAt(42, 50, "Tilt to switch!", &Font16, B);
  paint.DrawStringAt(15, 76,  "Stand -> Pet   Flat -> Sleep", &Font12, B);
  paint.DrawStringAt(15, 92,  "Tilt -> Info   Flip -> Focus", &Font12, B);
  paint.DrawStringAt(15, 108, "Face-down -> Pause",           &Font12, B);
  epd.Display(_fb);
}

void deepRefresh(int mode) {
  epd.Init(FULL);
  renderToBuffer(mode);
  epd.DisplayPartBaseImage(_fb);
  _partialCount = 0;
}

void fullRefresh(int mode) {
  epd.Init(FULL);
  renderToBuffer(mode);
  epd.DisplayPartBaseImage(_fb);
  _partialCount = 0;
}

void partialRefresh(int mode) {
  if (_partialCount >= PARTIAL_LIMIT) { fullRefresh(mode); return; }
  epd.Init(PART);
  renderToBuffer(mode);
  epd.DisplayPart(_fb);
  _partialCount++;
}

void sleepDisplay() { epd.Sleep(); }

// ═══════════════════════════════════════════════════════════
//  Eye drawing primitives  (big, circular)
// ═══════════════════════════════════════════════════════════

void thickHLine(int x, int y, int w, int t, int col) {
  for (int d = -t; d <= t; d++)
    paint.DrawHorizontalLine(x, y + d, w, col);
}

void thickLine(int x0, int y0, int x1, int y1, int t, int col) {
  for (int d = -t; d <= t; d++)
    paint.DrawLine(x0, y0 + d, x1, y1 + d, col);
}

/* normal open eye: black outline → white fill → pupil → sparkle */
void drawOpenEye(int cx, int cy, int R, int pR, int8_t pox) {
  paint.DrawFilledCircle(cx, cy, R,     B);
  paint.DrawFilledCircle(cx, cy, R - 3, W);
  paint.DrawFilledCircle(cx + pox, cy + 2, pR, B);
  int sx = cx + pox - pR / 3;
  int sy = cy + 2  - pR / 3;
  paint.DrawFilledCircle(sx, sy, pR / 3 + 1, W);
}

/* blink: thin horizontal bar */
void drawBlinkEye(int cx, int cy, int R) {
  paint.DrawFilledRectangle(cx - R, cy - 2, cx + R, cy + 2, B);
}

/* happy ^_^ : thick upward-V */
void drawHappyEye(int cx, int cy, int R) {
  int w = R, h = R * 2 / 3;
  thickLine(cx - w, cy, cx, cy - h, 2, B);
  thickLine(cx, cy - h, cx + w, cy, 2, B);
}

/* sleeping: gentle downward curve */
void drawSleepEyeCurve(int cx, int cy, int R) {
  thickLine(cx - R, cy, cx, cy + 4, 1, B);
  thickLine(cx, cy + 4, cx + R, cy, 1, B);
}

/* focused / squinted eye: circle with upper eyelid cropped */
void drawFocusedEye(int cx, int cy, int R, int pR) {
  paint.DrawFilledCircle(cx, cy, R, B);
  paint.DrawFilledCircle(cx, cy, R - 2, W);
  /* erase upper ~40 % → squint */
  paint.DrawFilledRectangle(cx - R - 1, cy - R - 1,
                            cx + R + 1, cy - R / 3, W);
  thickHLine(cx - R, cy - R / 3, R * 2, 1, B);   /* eyelid line */
  paint.DrawFilledCircle(cx, cy + 2, pR, B);       /* pupil */
  paint.DrawFilledCircle(cx - pR / 4, cy + 1 - pR / 4,
                         pR / 4 + 1, W);            /* sparkle */
}

// ═══════════════════════════════════════════════════════════
//  PET FACE  (landscape 250×122)
// ═══════════════════════════════════════════════════════════

void drawPetFace() {
  const int LX = 72, RX = 178, EY = 50;
  const int R  = 32, PR = 14;
  int8_t  pox   = getPetEyeOffsetX();
  uint8_t blink = getPetBlinkLevel();
  bool    happy = isPetHappyPhase();

  if (blink == 2) {
    drawBlinkEye(LX, EY, R);
    drawBlinkEye(RX, EY, R);
  } else if (happy) {
    drawHappyEye(LX, EY, R);
    drawHappyEye(RX, EY, R);
  } else {
    drawOpenEye(LX, EY, R, PR, pox);
    drawOpenEye(RX, EY, R, PR, pox);
  }

  /* small smile */
  paint.DrawFilledCircle(125, 90, 4, B);
  paint.DrawFilledCircle(125, 88, 4, W);

  /* expression label */
  const char* expr;
  if      (blink)    expr = "* blink *";
  else if (happy)    expr = "^_^  happy!";
  else if (pox < 0)  expr = "<  looking...";
  else if (pox > 0)  expr = "looking...  >";
  else               expr = "Hi there!";
  paint.DrawStringAt(70, 108, expr, &Font12, B);
}

// ═══════════════════════════════════════════════════════════
//  SLEEP FACE  (landscape 250×122, adaptive rotation)
// ═══════════════════════════════════════════════════════════

void drawSleepFace() {
  const int LX = 72, RX = 178, EY = 45;
  const int R = 28;

  drawSleepEyeCurve(LX, EY, R);
  drawSleepEyeCurve(RX, EY, R);

  /* mouth — small "o" */
  paint.DrawCircle(125, 74, 5, B);

  /* floating zzz */
  if (millis() - _sleepTimer > 800) {
    _sleepTimer = millis();
    _sleepFrame = (_sleepFrame + 1) % 3;
  }
  int bx = 190 + _sleepFrame * 4;
  int by = 26  - _sleepFrame * 3;
  paint.DrawStringAt(bx,      by,      "z", &Font12, B);
  paint.DrawStringAt(bx + 12, by - 12, "Z", &Font16, B);
  paint.DrawStringAt(bx + 28, by - 28, "Z", &Font20, B);

  paint.DrawStringAt(56, 102, "Shhh... sleeping", &Font12, B);
}

// ═══════════════════════════════════════════════════════════
//  TEMP & CALENDAR  (portrait 122×250)
// ═══════════════════════════════════════════════════════════

void drawTempCalPortrait() {
  /* dummy data — replace with real sensor readings later */
  const char* dayName = "SATURDAY";
  const char* dateStr = "Feb 21, 2026";
  const char* timeStr = "14:30";
  int  tempI = 22, tempF = 5;    /* 22.5 °C */
  int  hum   = 55;               /* 55 % */

  /* day & date */
  paint.DrawStringAt(10, 8,  dayName, &Font12, B);
  paint.DrawStringAt(10, 26, dateStr, &Font12, B);

  paint.DrawHorizontalLine(4, 44, 114, B);

  /* large time */
  paint.DrawStringAt(10, 54, timeStr, &Font24, B);

  paint.DrawHorizontalLine(4, 88, 114, B);

  /* temperature */
  paint.DrawStringAt(6, 96, "Temperature", &Font12, B);
  {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d.%d", tempI, tempF);
    paint.DrawStringAt(6, 114, buf, &Font24, B);
    /* degree + C */
    int tx = 6 + 17 * (int)strlen(buf);
    paint.DrawCircle(tx + 3, 116, 2, B);
    paint.DrawStringAt(tx + 8, 114, "C", &Font24, B);
  }

  /* temp bar */
  int barW = 100;
  paint.DrawRectangle(6, 146, 6 + barW, 158, B);
  int fill = (int)(barW * (tempI + tempF / 10.0f) / 45.0f);
  if (fill > barW) fill = barW;
  if (fill > 0) paint.DrawFilledRectangle(6, 146, 6 + fill, 158, B);

  paint.DrawHorizontalLine(4, 168, 114, B);

  /* humidity */
  paint.DrawStringAt(6, 176, "Humidity", &Font12, B);
  {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", hum);
    paint.DrawStringAt(6, 194, buf, &Font24, B);
  }

  /* humidity bar */
  paint.DrawRectangle(6, 224, 6 + barW, 236, B);
  int hfill = barW * hum / 100;
  if (hfill > 0) paint.DrawFilledRectangle(6, 224, 6 + hfill, 236, B);

  /* branding */
  paint.DrawStringAt(20, 244, "UniBuddy", &Font8, B);
}

// ═══════════════════════════════════════════════════════════
//  FOCUS / POMODORO  (landscape 250×122, ROTATE_90)
// ═══════════════════════════════════════════════════════════

void drawFocusScreen() {
  /* --- focused eyes at top --- */
  const int EL = 52, ER = 110, EY = 16, ER2 = 14, PR = 6;
  uint8_t blink = getPetBlinkLevel();

  if (blink == 2) {
    drawBlinkEye(EL, EY, ER2);
    drawBlinkEye(ER, EY, ER2);
  } else {
    drawFocusedEye(EL, EY, ER2, PR);
    drawFocusedEye(ER, EY, ER2, PR);
  }

  paint.DrawHorizontalLine(4, 34, 242, B);

  /* --- countdown --- */
  uint32_t sLeft = pomodoroSecondsLeft();
  int mn = sLeft / 60, sc = sLeft % 60;
  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", mn, sc);
  paint.DrawStringAt(70, 40, timeBuf, &Font24, B);

  /* --- progress bar --- */
  float progress = 1.0f - (float)sLeft / (POMODORO_DURATION / 1000.0f);
  if (progress < 0) progress = 0;
  if (progress > 1) progress = 1;
  int barX = 20, barY = 70, barW = 210, barH = 14;
  paint.DrawRectangle(barX, barY, barX + barW, barY + barH, B);
  int fw = (int)(barW * progress);
  if (fw > 0)
    paint.DrawFilledRectangle(barX + 1, barY + 1,
                              barX + fw, barY + barH - 1, B);

  /* --- session dots (bottom-left) --- */
  uint8_t sess = getSessionCount();
  for (int i = 0; i < 4; i++) {
    int dx = 20 + i * 16;
    if ((int)i < (int)(sess % 4))
      paint.DrawFilledCircle(dx, 100, 4, B);
    else
      paint.DrawCircle(dx, 100, 4, B);
  }

  /* --- "FOCUS #N" small at bottom-right --- */
  {
    char lb[16];
    snprintf(lb, sizeof(lb), "FOCUS #%d", sess + 1);
    paint.DrawStringAt(160, 102, lb, &Font12, B);
  }
}

// ═══════════════════════════════════════════════════════════
//  BREAK  (landscape 250×122, ROTATE_90)
// ═══════════════════════════════════════════════════════════

void drawBreakScreen() {
  paint.DrawStringAt(52, 4, "BREAK TIME", &Font20, B);
  paint.DrawHorizontalLine(4, 28, 242, B);

  uint32_t sLeft = breakSecondsLeft();
  int mn = sLeft / 60, sc = sLeft % 60;
  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", mn, sc);
  paint.DrawStringAt(70, 36, timeBuf, &Font24, B);

  {
    char buf[24];
    snprintf(buf, sizeof(buf), "Cycle %d done!", getCompletedCycleCount());
    paint.DrawStringAt(60, 72, buf, &Font12, B);
  }

  /* relaxed eyes: happy expression */
  drawHappyEye(80, 100, 16);
  drawHappyEye(170, 100, 16);
}

// ═══════════════════════════════════════════════════════════
//  Render dispatcher
// ═══════════════════════════════════════════════════════════

void renderToBuffer(int mode) {
  paint.Clear(W);

  switch (mode) {
    case MODE_PET:        drawPetFace();        break;
    case MODE_SLEEP:      drawSleepFace();      break;
    case MODE_TEMPTIME_L:
    case MODE_TEMPTIME_R: drawTempCalPortrait(); break;
    case MODE_POMODORO:   drawFocusScreen();    break;
    case MODE_BREAK:      drawBreakScreen();    break;
    default: break;
  }
}
