#pragma once
/*
 * ============================================================
 *  epaper.h — E-Paper driver & multi-screen renderer
 *  Waveshare 2.13" V4 (250×122, BW)
 *
 *  Every mood has a unique eye style.
 *  Focus screen: big eyes top → progress bar → time → dots+label
 * ============================================================
 */

#include <SPI.h>
#include "config.h"
#include "epd2in13_V4.h"
#include "epdpaint.h"
#include "pet.h"
#include "pomodoro.h"
#include "behaviour.h"

#define COL_BLACK  0
#define COL_WHITE  1
#define B  COL_BLACK
#define W  COL_WHITE

static Epd epd;
static unsigned char _fb[128 / 8 * 250];
static Paint paint(_fb, 128, 250);

static int  _partialCount = 0;
static const int PARTIAL_LIMIT = 30;

static uint8_t  _sleepFrame  = 0;
static uint32_t _sleepTimer  = 0;

// forward decls
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
  if (epd.Init(FULL) != 0) { Serial.println(F("[EPD] FAIL")); return; }
  epd.Clear();
  paint.SetRotate(ROTATE_270);
  Serial.println(F("[EPD] Ready"));
}

void setDisplayRotation(int r) { paint.SetRotate(r); }

void showSplashScreen() {
  paint.Clear(W);
  paint.DrawStringAt(40, 12, "UniBuddy", &Font24, B);
  paint.DrawHorizontalLine(30, 42, 190, B);
  paint.DrawStringAt(42, 50, "Tilt to switch!", &Font16, B);
  paint.DrawStringAt(15, 76,  "Stand -> Pet   Flat -> Sleep", &Font12, B);
  paint.DrawStringAt(15, 92,  "Tilt -> Info   Flip -> Focus", &Font12, B);
  paint.DrawStringAt(15, 108, "Shake me in Pet mode!", &Font12, B);
  epd.Display(_fb);
}

void deepRefresh(int mode) {
  epd.Init(FULL); renderToBuffer(mode);
  epd.DisplayPartBaseImage(_fb); _partialCount = 0;
}
void fullRefresh(int mode) {
  epd.Init(FULL); renderToBuffer(mode);
  epd.DisplayPartBaseImage(_fb); _partialCount = 0;
}
void partialRefresh(int mode) {
  if (_partialCount >= PARTIAL_LIMIT) { fullRefresh(mode); return; }
  epd.Init(PART); renderToBuffer(mode);
  epd.DisplayPart(_fb); _partialCount++;
}
void sleepDisplay() { epd.Sleep(); }

// ═══════════════════════════════════════════════════════════
//  Drawing primitives
// ═══════════════════════════════════════════════════════════

void thickHLine(int x, int y, int w, int t, int col) {
  for (int d = -t; d <= t; d++) paint.DrawHorizontalLine(x, y + d, w, col);
}

void thickLine(int x0, int y0, int x1, int y1, int t, int col) {
  for (int d = -t; d <= t; d++) paint.DrawLine(x0, y0 + d, x1, y1 + d, col);
}

// ═══════════════════════════════════════════════════════════
//  Mood-specific eye styles  (all take centre x,y + radius)
// ═══════════════════════════════════════════════════════════

/* standard open: outline→white→pupil→sparkle */
void eyeOpen(int cx, int cy, int R, int pR, int8_t pox) {
  paint.DrawFilledCircle(cx, cy, R, B);
  paint.DrawFilledCircle(cx, cy, R - 3, W);
  paint.DrawFilledCircle(cx + pox, cy + 2, pR, B);
  paint.DrawFilledCircle(cx + pox - pR/3, cy + 2 - pR/3, pR/3 + 1, W);
}

/* blink: thin bar */
void eyeBlink(int cx, int cy, int R) {
  paint.DrawFilledRectangle(cx - R, cy - 2, cx + R, cy + 2, B);
}

/* happy ^_^ */
void eyeHappy(int cx, int cy, int R) {
  thickLine(cx - R, cy, cx, cy - R*2/3, 2, B);
  thickLine(cx, cy - R*2/3, cx + R, cy, 2, B);
}

/* cute  ⌒‿⌒  (happy arc + sparkle dot above) */
void eyeCute(int cx, int cy, int R) {
  eyeHappy(cx, cy, R);
  /* sparkle: two small dots */
  paint.DrawFilledCircle(cx - R/2, cy - R + 2, 2, B);
  paint.DrawFilledCircle(cx + R/2, cy - R + 2, 2, B);
}

/* interested: bigger pupil, slightly wider */
void eyeInterested(int cx, int cy, int R, int8_t pox) {
  paint.DrawFilledCircle(cx, cy, R, B);
  paint.DrawFilledCircle(cx, cy, R - 3, W);
  int bigP = R * 2 / 3;
  paint.DrawFilledCircle(cx + pox, cy + 1, bigP, B);
  paint.DrawFilledCircle(cx + pox - bigP/3, cy - bigP/4, bigP/3 + 1, W);
}

/* bored: half-lid, small pupil off-centre */
void eyeBored(int cx, int cy, int R) {
  paint.DrawFilledCircle(cx, cy, R, B);
  paint.DrawFilledCircle(cx, cy, R - 3, W);
  /* eyelid covers upper 50% */
  paint.DrawFilledRectangle(cx - R - 1, cy - R - 1, cx + R + 1, cy, W);
  thickHLine(cx - R, cy, R * 2, 1, B);
  paint.DrawFilledCircle(cx + R/3, cy + R/4, R/4, B);  /* small pupil, off-right */
}

/* surprised  O_O: extra wide, tiny far-apart pupils */
void eyeSurprised(int cx, int cy, int R) {
  int bigR = R + 4;
  paint.DrawFilledCircle(cx, cy, bigR, B);
  paint.DrawFilledCircle(cx, cy, bigR - 3, W);
  paint.DrawFilledCircle(cx, cy, R/3, B);
  paint.DrawFilledCircle(cx - 2, cy - 2, 2, W);
}

/* worried: slightly droopy, brow line angled down-inward */
void eyeWorried(int cx, int cy, int R, bool isLeft) {
  paint.DrawFilledCircle(cx, cy, R, B);
  paint.DrawFilledCircle(cx, cy, R - 3, W);
  paint.DrawFilledCircle(cx, cy + 2, R/3, B);
  paint.DrawFilledCircle(cx - 1, cy, 2, W);
  /* worried brow */
  if (isLeft)
    thickLine(cx - R, cy - R - 4, cx + R/2, cy - R + 2, 1, B);
  else
    thickLine(cx - R/2, cy - R + 2, cx + R, cy - R - 4, 1, B);
}

/* annoyed  >_< */
void eyeAnnoyed(int cx, int cy, int R) {
  thickLine(cx - R, cy - R/2, cx, cy, 2, B);
  thickLine(cx, cy, cx + R, cy - R/2, 2, B);
  thickLine(cx - R, cy + R/2, cx, cy, 2, B);
  thickLine(cx, cy, cx + R, cy + R/2, 2, B);
}

/* dizzy  @_@: spiral-like concentric rings */
void eyeDizzy(int cx, int cy, int R) {
  paint.DrawCircle(cx, cy, R, B);
  paint.DrawCircle(cx, cy, R * 2 / 3, B);
  paint.DrawCircle(cx, cy, R / 3, B);
  paint.DrawFilledCircle(cx, cy, 3, B);
}

/* sad: droopy outline + small pupil low */
void eyeSad(int cx, int cy, int R) {
  paint.DrawFilledCircle(cx, cy, R, B);
  paint.DrawFilledCircle(cx, cy, R - 3, W);
  /* droopy top: erase top-outer corner+ redraw arc */
  paint.DrawFilledRectangle(cx - R - 1, cy - R - 1, cx + R + 1, cy - R/2, W);
  thickLine(cx - R, cy - R/3, cx + R, cy - R/2, 1, B);
  paint.DrawFilledCircle(cx, cy + R/4, R/4, B);
  paint.DrawFilledCircle(cx - 1, cy + R/4 - 2, 2, W);
}

/* angry: V brows + sharp pupil */
void eyeAngry(int cx, int cy, int R, bool isLeft) {
  paint.DrawFilledCircle(cx, cy, R, B);
  paint.DrawFilledCircle(cx, cy, R - 3, W);
  paint.DrawFilledCircle(cx, cy + 2, R/3 + 1, B);
  /* V brow */
  if (isLeft)
    thickLine(cx - R, cy - R + 6, cx + R/2, cy - R - 2, 2, B);
  else
    thickLine(cx - R/2, cy - R - 2, cx + R, cy - R + 6, 2, B);
}

/* confused: one brow up, one down */
void eyeConfused(int cx, int cy, int R, bool isLeft) {
  paint.DrawFilledCircle(cx, cy, R, B);
  paint.DrawFilledCircle(cx, cy, R - 3, W);
  paint.DrawFilledCircle(cx, cy + 1, R/3, B);
  paint.DrawFilledCircle(cx - 1, cy - 1, 2, W);
  if (isLeft)
    thickLine(cx - R, cy - R - 2, cx + R/2, cy - R + 4, 1, B);
  else
    thickLine(cx - R/2, cy - R + 4, cx + R, cy - R - 6, 1, B);
}

/* focused: squinted half-circle */
void eyeFocused(int cx, int cy, int R, int pR) {
  paint.DrawFilledCircle(cx, cy, R, B);
  paint.DrawFilledCircle(cx, cy, R - 2, W);
  paint.DrawFilledRectangle(cx - R - 1, cy - R - 1, cx + R + 1, cy - R/3, W);
  thickHLine(cx - R, cy - R/3, R * 2, 1, B);
  paint.DrawFilledCircle(cx, cy + 2, pR, B);
  paint.DrawFilledCircle(cx - pR/4, cy + 1 - pR/4, pR/4 + 1, W);
}

/* tired: heavy lids */
void eyeTired(int cx, int cy, int R) {
  paint.DrawFilledCircle(cx, cy, R, B);
  paint.DrawFilledCircle(cx, cy, R - 3, W);
  /* heavy eyelid covers upper 60% */
  paint.DrawFilledRectangle(cx - R - 1, cy - R - 1, cx + R + 1, cy + R/6, W);
  thickHLine(cx - R, cy + R/6, R * 2, 1, B);
  paint.DrawFilledCircle(cx, cy + R/3, R/4, B);
}

/* asleep: gentle closed curves */
void eyeAsleep(int cx, int cy, int R) {
  thickLine(cx - R, cy, cx, cy + 4, 1, B);
  thickLine(cx, cy + 4, cx + R, cy, 1, B);
}

// ═══════════════════════════════════════════════════════════
//  Composite eye drawer — dispatches per mood
// ═══════════════════════════════════════════════════════════

void drawEyePair(int lx, int rx, int ey, int R, int pR) {
  PetMood mood  = getPetMood();
  int8_t  pox   = getPetEyeOffsetX();
  uint8_t blink = getPetBlinkLevel();
  bool    spec  = isPetSpecialPhase();

  /* always blink on phase 3, regardless of mood */
  if (blink == 2) {
    eyeBlink(lx, ey, R);
    eyeBlink(rx, ey, R);
    return;
  }

  /* mood-specific special on phase 7 */
  if (spec) {
    switch (mood) {
      case MOOD_HAPPY:
      case MOOD_CUTE:
        eyeCute(lx, ey, R); eyeCute(rx, ey, R); return;
      case MOOD_SURPRISED:
        eyeSurprised(lx, ey, R); eyeSurprised(rx, ey, R); return;
      case MOOD_ANNOYED:
        eyeAnnoyed(lx, ey, R); eyeAnnoyed(rx, ey, R); return;
      case MOOD_DIZZY:
        eyeDizzy(lx, ey, R); eyeDizzy(rx, ey, R); return;
      default: break;  /* fall through to normal for other moods */
    }
  }

  /* normal frames: mood-aware eye style */
  switch (mood) {
    case MOOD_HAPPY:
      eyeOpen(lx, ey, R, pR, pox); eyeOpen(rx, ey, R, pR, pox); break;
    case MOOD_CUTE:
      eyeOpen(lx, ey, R, pR, pox); eyeOpen(rx, ey, R, pR, pox);
      /* blush dots under eyes */
      paint.DrawFilledCircle(lx - R/2, ey + R + 4, 3, B);
      paint.DrawFilledCircle(lx + R/2, ey + R + 4, 3, B);
      paint.DrawFilledCircle(rx - R/2, ey + R + 4, 3, B);
      paint.DrawFilledCircle(rx + R/2, ey + R + 4, 3, B);
      break;
    case MOOD_INTERESTED:
      eyeInterested(lx, ey, R, pox); eyeInterested(rx, ey, R, pox); break;
    case MOOD_BORED:
      eyeBored(lx, ey, R); eyeBored(rx, ey, R); break;
    case MOOD_SURPRISED:
      eyeSurprised(lx, ey, R); eyeSurprised(rx, ey, R); break;
    case MOOD_WORRIED:
      eyeWorried(lx, ey, R, true); eyeWorried(rx, ey, R, false); break;
    case MOOD_ANNOYED:
      eyeAnnoyed(lx, ey, R); eyeAnnoyed(rx, ey, R); break;
    case MOOD_DIZZY:
      eyeDizzy(lx, ey, R); eyeDizzy(rx, ey, R); break;
    case MOOD_SAD:
      eyeSad(lx, ey, R); eyeSad(rx, ey, R); break;
    case MOOD_ANGRY:
      eyeAngry(lx, ey, R, true); eyeAngry(rx, ey, R, false); break;
    case MOOD_CONFUSED:
      eyeConfused(lx, ey, R, true); eyeConfused(rx, ey, R, false); break;
    case MOOD_FOCUSED:
      eyeFocused(lx, ey, R, pR); eyeFocused(rx, ey, R, pR); break;
    case MOOD_TIRED:
      eyeTired(lx, ey, R); eyeTired(rx, ey, R); break;
    case MOOD_ASLEEP:
      eyeAsleep(lx, ey, R); eyeAsleep(rx, ey, R); break;
    default:
      eyeOpen(lx, ey, R, pR, pox); eyeOpen(rx, ey, R, pR, pox); break;
  }
}

// ═══════════════════════════════════════════════════════════
//  Mood symbol — shapes only, no text labels
//  Drawn near the eyes to express emotion
// ═══════════════════════════════════════════════════════════

/* small heart shape at (cx, cy) */
void drawHeart(int cx, int cy, int s) {
  paint.DrawFilledCircle(cx - s, cy, s, B);
  paint.DrawFilledCircle(cx + s, cy, s, B);
  /* V bottom */
  for (int row = 0; row <= s * 2; row++) {
    int hw = s * 2 - row;  /* half-width narrows */
    if (hw < 0) hw = 0;
    paint.DrawHorizontalLine(cx - hw, cy + row, hw * 2 + 1, B);
  }
}

/* manga anger cross ╳ at (cx, cy) */
void drawAngerMark(int cx, int cy, int s) {
  thickLine(cx - s, cy - s, cx + s, cy + s, 1, B);
  thickLine(cx + s, cy - s, cx - s, cy + s, 1, B);
}

/* sweat drop at (cx, cy) */
void drawSweatDrop(int cx, int cy, int s) {
  paint.DrawFilledCircle(cx, cy + s, s, B);
  paint.DrawLine(cx, cy - s, cx - s, cy + s, B);
  paint.DrawLine(cx, cy - s, cx + s, cy + s, B);
}

/* sparkle ✦ four-pointed star */
void drawSparkle(int cx, int cy, int s) {
  paint.DrawLine(cx, cy - s, cx, cy + s, B);
  paint.DrawLine(cx - s, cy, cx + s, cy, B);
  paint.DrawFilledCircle(cx, cy, s/3, B);
}

/* spiral @ */
void drawSpiral(int cx, int cy, int R) {
  paint.DrawCircle(cx, cy, R, B);
  paint.DrawCircle(cx, cy, R * 2/3, B);
  paint.DrawCircle(cx + 2, cy - 1, R/3, B);
}

void drawMoodSymbol(int rx, int ey, int R) {
  PetMood mood = getPetMood();
  int sx = rx + R + 8;    /* symbol to the right of right eye */
  int sy = ey - R/2;

  switch (mood) {
    case MOOD_HAPPY:
      break;  /* eyes alone are enough */
    case MOOD_CUTE:
      /* two small sparkles */
      drawSparkle(sx,     sy - 4, 5);
      drawSparkle(sx + 14, sy + 2, 4);
      break;
    case MOOD_SURPRISED:
      /* !! exclamation marks */
      paint.DrawFilledRectangle(sx, sy - 6, sx + 3, sy + 6, B);
      paint.DrawFilledCircle(sx + 1, sy + 10, 2, B);
      paint.DrawFilledRectangle(sx + 8, sy - 6, sx + 11, sy + 6, B);
      paint.DrawFilledCircle(sx + 9, sy + 10, 2, B);
      break;
    case MOOD_WORRIED:
      drawSweatDrop(sx + 4, sy, 4);  /* 💧 */
      break;
    case MOOD_ANNOYED:
      drawAngerMark(sx + 4, sy, 6);  /* ╳ */
      break;
    case MOOD_ANGRY:
      /* double anger marks */
      drawAngerMark(sx,      sy - 4, 5);
      drawAngerMark(sx + 14, sy + 2, 4);
      break;
    case MOOD_DIZZY:
      break;  /* spiral eyes are enough */
    case MOOD_SAD:
      /* teardrop under right eye */
      drawSweatDrop(rx + R/2, ey + R + 4, 3);
      break;
    case MOOD_CONFUSED:
      /* ? mark using shapes */
      paint.DrawCircle(sx + 4, sy - 2, 5, B);
      paint.DrawFilledRectangle(sx + 7, sy - 2, sx + 9, sy + 6, B);
      paint.DrawFilledCircle(sx + 8, sy + 10, 2, B);
      break;
    case MOOD_TIRED:
      /* small zzz */
      paint.DrawStringAt(sx, sy - 4, "z", &Font8, B);
      paint.DrawStringAt(sx + 8, sy - 10, "z", &Font12, B);
      break;
    default:
      break;  /* neutral / focused / asleep / bored / interested: no symbol */
  }
}

// ═══════════════════════════════════════════════════════════
//  PET FACE  (landscape 250×122)
// ═══════════════════════════════════════════════════════════

void drawPetFace() {
  /* eyes centred vertically on 122px screen */
  const int LX = 78, RX = 172, EY = 52;
  const int R = 32, PR = 14;

  drawEyePair(LX, RX, EY, R, PR);
  drawMoodSymbol(RX, EY, R);
}

// ═══════════════════════════════════════════════════════════
//  SLEEP FACE  (landscape 250×122, adaptive rotation)
// ═══════════════════════════════════════════════════════════

void drawSleepFace() {
  const int LX = 78, RX = 172, EY = 52;
  const int R = 28;

  eyeAsleep(LX, EY, R);
  eyeAsleep(RX, EY, R);

  /* floating zzz */
  if (millis() - _sleepTimer > 800) {
    _sleepTimer = millis();
    _sleepFrame = (_sleepFrame + 1) % 3;
  }
  int bx = 188 + _sleepFrame * 5;
  int by = 36  - _sleepFrame * 3;
  paint.DrawStringAt(bx,      by,      "z", &Font12, B);
  paint.DrawStringAt(bx + 12, by - 10, "z", &Font16, B);
  paint.DrawStringAt(bx + 26, by - 22, "z", &Font20, B);
}

// ═══════════════════════════════════════════════════════════
//  TEMP & CALENDAR  (portrait 122×250)
// ═══════════════════════════════════════════════════════════

void drawTempCalPortrait() {
  const char* dayName = "SATURDAY";
  const char* dateStr = "Feb 21, 2026";
  const char* timeStr = "14:30";
  int tempI = 22, tempF = 5;
  int hum = 55;

  paint.DrawStringAt(10, 8,  dayName, &Font12, B);
  paint.DrawStringAt(10, 26, dateStr, &Font12, B);
  paint.DrawHorizontalLine(4, 44, 114, B);

  paint.DrawStringAt(10, 54, timeStr, &Font24, B);
  paint.DrawHorizontalLine(4, 88, 114, B);

  paint.DrawStringAt(6, 96, "Temperature", &Font12, B);
  {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d.%d", tempI, tempF);
    paint.DrawStringAt(6, 114, buf, &Font24, B);
    int tx = 6 + 17 * (int)strlen(buf);
    paint.DrawCircle(tx + 3, 116, 2, B);
    paint.DrawStringAt(tx + 8, 114, "C", &Font24, B);
  }

  int barW = 100;
  paint.DrawRectangle(6, 146, 6 + barW, 158, B);
  int fill = (int)(barW * (tempI + tempF / 10.0f) / 45.0f);
  if (fill > barW) fill = barW;
  if (fill > 0) paint.DrawFilledRectangle(6, 146, 6 + fill, 158, B);

  paint.DrawHorizontalLine(4, 168, 114, B);
  paint.DrawStringAt(6, 176, "Humidity", &Font12, B);
  {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", hum);
    paint.DrawStringAt(6, 194, buf, &Font24, B);
  }

  paint.DrawRectangle(6, 224, 6 + barW, 236, B);
  int hfill = barW * hum / 100;
  if (hfill > 0) paint.DrawFilledRectangle(6, 224, 6 + hfill, 236, B);

  paint.DrawStringAt(20, 244, "UniBuddy", &Font8, B);
}

// ═══════════════════════════════════════════════════════════
//  FOCUS / POMODORO  (landscape 250×122, ROTATE_90)
//
//  Layout (per mockup):
//   ┌───────────────────────────────┐
//   │  [focused eye L] [focused eye R]  │  ← big squint eyes (~y 8..50)
//   │                                  │
//   │  ████████████░░░░░░░░░░░░░░░░░░  │  ← progress bar (y 56..70)
//   │           20:24                   │  ← time below bar (y 76)
//   │  ◉◉◉○ Session 1        FOCUS     │  ← dots + label at very bottom
//   └───────────────────────────────┘
// ═══════════════════════════════════════════════════════════

void drawFocusScreen() {
  /* --- focused eyes with subtle drift --- */
  const int EL = 78, ER = 172, EY = 26;
  const int R = 22, PR = 9;
  uint8_t blink = getPetBlinkLevel();

  /* slow pupil drift: oscillates +/-2px every ~8 seconds */
  static const int8_t _drift[] = {0, 1, 2, 1, 0, -1, -2, -1};
  int8_t pdx = _drift[(millis() / 1000) % 8];

  if (blink == 2) {
    eyeBlink(EL, EY, R);
    eyeBlink(ER, EY, R);
  } else {
    /* draw both focused eyes with drift */
    int exs[2] = {EL, ER};
    for (int i = 0; i < 2; i++) {
      int cx = exs[i];
      paint.DrawFilledCircle(cx, EY, R, B);
      paint.DrawFilledCircle(cx, EY, R - 2, W);
      paint.DrawFilledRectangle(cx - R - 1, EY - R - 1, cx + R + 1, EY - R/3, W);
      thickHLine(cx - R, EY - R/3, R * 2, 1, B);
      paint.DrawFilledCircle(cx + pdx, EY + 2, PR, B);
      paint.DrawFilledCircle(cx + pdx - PR/4, EY + 1 - PR/4, PR/4 + 1, W);
    }
  }

  /* --- progress bar (no divider) --- */
  uint32_t sLeft = pomodoroSecondsLeft();
  float progress = 1.0f - (float)sLeft / (POMODORO_DURATION / 1000.0f);
  if (progress < 0) progress = 0;
  if (progress > 1) progress = 1;
  int barX = 16, barY = 54, barW = 218, barH = 12;
  paint.DrawRectangle(barX, barY, barX + barW, barY + barH, B);
  int fw = (int)(barW * progress);
  if (fw > 0)
    paint.DrawFilledRectangle(barX + 1, barY + 1,
                              barX + fw, barY + barH - 1, B);

  /* --- time below progress bar --- */
  int mn = sLeft / 60, sc = sLeft % 60;
  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", mn, sc);
  paint.DrawStringAt(84, 72, timeBuf, &Font24, B);

  /* --- bottom row: closer to bottom margin --- */
  uint8_t sess = getSessionCount();
  int by = 110;   /* Font12 occupies by..by+12 → 110..122 */

  /* "Session N" at bottom-left */
  {
    char sb[16];
    snprintf(sb, sizeof(sb), "Session %d", sess + 1);
    paint.DrawStringAt(6, by, sb, &Font12, B);
  }

  /* session dots after label */
  for (int i = 0; i < 4; i++) {
    int dotX = 110 + i * 14;
    if ((int)i < (int)(sess % 4))
      paint.DrawFilledCircle(dotX, by + 6, 4, B);
    else
      paint.DrawCircle(dotX, by + 6, 4, B);
  }

  /* "FOCUS" at bottom-right */
  paint.DrawStringAt(194, by, "FOCUS", &Font12, B);
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

  eyeHappy(80, 100, 16);
  eyeHappy(170, 100, 16);
}

// ═══════════════════════════════════════════════════════════
//  Render dispatcher
// ═══════════════════════════════════════════════════════════

void renderToBuffer(int mode) {
  paint.Clear(W);
  switch (mode) {
    case MODE_PET:        drawPetFace();       break;
    case MODE_SLEEP:      drawSleepFace();     break;
    case MODE_TEMPTIME_L:
    case MODE_TEMPTIME_R: drawTempCalPortrait(); break;
    case MODE_POMODORO:   drawFocusScreen();   break;
    case MODE_BREAK:      drawBreakScreen();   break;
    default: break;
  }
}
