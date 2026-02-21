/*
 * ============================================================
 *  MovementTest — Tilt-based multi-screen display
 *  Modulino Movement (LSM6DSOX) + Waveshare 2.13" V4 E-Paper
 *
 *  Wiring:
 *    IMU  → QWIIC (I²C)
 *    EPD  → VCC 5V | GND | DIN D11 | CLK D13 |
 *           CS D10 | DC D9 | RST D8 | BUSY D7
 *
 *  Tilt orientations  (roll = atan2(aY, aZ)):
 *    Roll < -70       → Pet face    (cute animated eyes)
 *    -25 < Roll < 25  → Sleep       (zzz)
 *    Roll > 70        → Pomodoro    (countdown timer, no eyes)
 *    Other roll       → Temp & Cal  (dummy data dashboard)
 *    accZ < -0.5      → Face-down   (display unchanged)
 *
 *  Display auto-rotates (ROTATE_270 / ROTATE_90) so text is
 *  always readable from the user's perspective.
 * ============================================================
 */

#include <SPI.h>
#include <math.h>
#include "Modulino.h"
#include "epd2in13_V4.h"
#include "epdpaint.h"

/* ── constants ─────────────────────────────────────────────── */
#define B   COL_BLACK
#define W   COL_WHITE
#define COL_BLACK  0
#define COL_WHITE  1
#define DW  250        // display width  (landscape)
#define DH  122        // display height (landscape)

/* ── hardware objects ──────────────────────────────────────── */
unsigned char framebuf[128 / 8 * 250];        // 4000 bytes
Paint paint(framebuf, 128, 250);
Epd   epd;
ModulinoMovement movement;

/* ── tilt modes ────────────────────────────────────────────── */
enum TiltMode { MODE_PET, MODE_SLEEP, MODE_TEMPTIME, MODE_POMODORO, MODE_FACEDOWN };

TiltMode curMode      = MODE_PET;
TiltMode prevDrawMode = MODE_PET;          // last mode actually rendered
int      curRotation  = ROTATE_270;

/* ── sensor ────────────────────────────────────────────────── */
float accX, accY, accZ;
float rollDeg, pitchDeg;

/* ── pet eye animation ─────────────────────────────────────── */
//   phase: 0 normal → 1 lookL → 2 normal → 3 blink
//        → 4 normal → 5 lookR → 6 normal → 7 happy
static const uint8_t  N_PHASES       = 8;
static const uint16_t PHASE_MS[8]    = {2000, 800, 1500, 300,
                                         2000, 800, 1500, 1200};
uint8_t       animPhase   = 0;
unsigned long animTimer   = 0;
int8_t        pupilOX     = 0;       // pupil horizontal offset
bool          eyesClosed  = false;
bool          eyesHappy   = false;

/* ── sleep animation ───────────────────────────────────────── */
uint8_t       sleepFrame  = 0;       // 0-2 cycles through "z" positions
unsigned long sleepTimer  = 0;

/* ── pomodoro state ────────────────────────────────────────── */
bool          pomoRunning    = false;
unsigned long pomoStartMs    = 0;
uint32_t      pomoDurationMs = 25UL * 60 * 1000;   // 25 min
bool          pomoIsBreak    = false;
uint8_t       pomoSession    = 1;

/* ── refresh tracking ──────────────────────────────────────── */
int  partialCount      = 0;
const int PARTIAL_LIMIT = 30;
bool forceFullRefresh   = true;

/* ── mode-change debounce ──────────────────────────────────── */
TiltMode pendingMode    = MODE_PET;
uint8_t  pendingCount   = 0;
const uint8_t DEBOUNCE_N = 3;

/* ─────────────────────────────────────────────────────────── */
/*                       SETUP                                 */
/* ─────────────────────────────────────────────────────────── */
void setup() {
  Serial.begin(115200);
  Serial.println(F("[MovementTest] Tilt multi-screen"));

  if (epd.Init(FULL) != 0) { Serial.println(F("[EPD] FAIL")); while (1); }
  epd.Clear();
  paint.SetRotate(ROTATE_270);

  Modulino.begin();
  movement.begin();
  Serial.println(F("[IMU] ready"));

  // ── splash ──
  paint.Clear(W);
  paint.DrawRectangle(0, 0, 249, 121, B);
  paint.DrawStringAt(14, 10, "MovementTest", &Font24, B);
  paint.DrawHorizontalLine(10, 42, 230, B);
  paint.DrawStringAt(10, 52, "Tilt to switch!", &Font16, B);
  paint.DrawStringAt(10, 76,  "Stand->Pet  Flat->Sleep", &Font12, B);
  paint.DrawStringAt(10, 92,  "Tilt->Info  Flip->Pomo",  &Font12, B);
  paint.DrawStringAt(10, 108, "Face-down->Pause",         &Font12, B);
  epd.DisplayPartBaseImage(framebuf);
  delay(3000);

  animTimer  = millis();
  sleepTimer = millis();
}

/* ─────────────────────────────────────────────────────────── */
/*                   ORIENTATION DETECT                        */
/* ─────────────────────────────────────────────────────────── */
void detectMode() {
  movement.update();
  accX = movement.getX();
  accY = movement.getY();
  accZ = movement.getZ();
  rollDeg  = atan2(accY, accZ) * 180.0 / M_PI;
  pitchDeg = atan2(-accX, sqrt(accY*accY + accZ*accZ)) * 180.0 / M_PI;

  TiltMode det;
  if (accZ < -0.5)            det = MODE_FACEDOWN;
  else if (rollDeg < -70)     det = MODE_PET;
  else if (rollDeg >  70)     det = MODE_POMODORO;
  else if (rollDeg > -25 && rollDeg < 25) det = MODE_SLEEP;
  else                        det = MODE_TEMPTIME;

  // debounce: need DEBOUNCE_N consecutive same readings
  if (det != curMode) {
    if (det == pendingMode) {
      if (++pendingCount >= DEBOUNCE_N) {
        prevDrawMode = curMode;
        curMode = det;
        forceFullRefresh = true;

        // rotation: ROTATE_90 when display is flipped
        if (curMode == MODE_POMODORO ||
            (curMode == MODE_TEMPTIME && rollDeg > 0))
          curRotation = ROTATE_90;
        else
          curRotation = ROTATE_270;
        paint.SetRotate(curRotation);

        // auto-start pomodoro timer on enter
        if (curMode == MODE_POMODORO && !pomoRunning) {
          pomoRunning  = true;
          pomoStartMs  = millis();
          pomoIsBreak  = false;
        }
        Serial.print(F("[Mode] ")); Serial.println(curMode);
      }
    } else {
      pendingMode  = det;
      pendingCount = 1;
    }
  } else {
    pendingCount = 0;
  }
}

/* ─────────────────────────────────────────────────────────── */
/*                   EYE DRAWING PRIMITIVES                    */
/* ─────────────────────────────────────────────────────────── */

// Thick line helper (width = 2*t+1)
void thickLine(int x0, int y0, int x1, int y1, int t, int col) {
  for (int d = -t; d <= t; d++)
    paint.DrawLine(x0, y0 + d, x1, y1 + d, col);
}

// Normal open eye  (outline → white fill → pupil → highlight)
void drawOpenEye(int cx, int cy, int R, int pR, int8_t pox) {
  paint.DrawFilledCircle(cx, cy, R,     B);        // outline
  paint.DrawFilledCircle(cx, cy, R - 3, W);        // white
  paint.DrawFilledCircle(cx + pox, cy + 2, pR, B); // pupil
  paint.DrawFilledCircle(cx + pox - pR/3,           // sparkle
                         cy + 2 - pR/3, pR/3 + 1, W);
}

// Blink (thin horizontal bar)
void drawBlinkEye(int cx, int cy, int R) {
  paint.DrawFilledRectangle(cx - R, cy - 2, cx + R, cy + 2, B);
}

// Happy ^_^ eye (upward arc made from thick V)
void drawHappyEye(int cx, int cy, int R) {
  int w = R;
  int h = R * 2 / 3;
  thickLine(cx - w, cy, cx, cy - h, 2, B);
  thickLine(cx, cy - h, cx + w, cy, 2, B);
}

// Sleeping eye — gently curved closed line
void drawSleepEye(int cx, int cy, int R) {
  int w = R;
  thickLine(cx - w, cy, cx, cy + 4, 1, B);
  thickLine(cx, cy + 4, cx + w, cy, 1, B);
}

/* ─────────────────────────────────────────────────────────── */
/*                    PET FACE SCREEN                          */
/* ─────────────────────────────────────────────────────────── */
void tickPetAnim() {
  if (millis() - animTimer >= PHASE_MS[animPhase]) {
    animTimer = millis();
    animPhase = (animPhase + 1) % N_PHASES;

    switch (animPhase) {
      case 1: pupilOX = -8;  eyesClosed = false; eyesHappy = false; break; // look L
      case 3: pupilOX =  0;  eyesClosed = true;  eyesHappy = false; break; // blink
      case 5: pupilOX =  8;  eyesClosed = false; eyesHappy = false; break; // look R
      case 7: pupilOX =  0;  eyesClosed = false; eyesHappy = true;  break; // happy
      default: pupilOX = 0;  eyesClosed = false; eyesHappy = false; break; // normal
    }
  }
}

void drawPetFace() {
  tickPetAnim();

  const int LX = 72,  RX = 178, EY = 55;   // eye centres
  const int R  = 32,  PR = 14;              // outer / pupil radius

  // ── draw left & right eye ──
  if (eyesClosed) {
    drawBlinkEye(LX, EY, R);
    drawBlinkEye(RX, EY, R);
  } else if (eyesHappy) {
    drawHappyEye(LX, EY, R);
    drawHappyEye(RX, EY, R);
  } else {
    drawOpenEye(LX, EY, R, PR, pupilOX);
    drawOpenEye(RX, EY, R, PR, pupilOX);
  }

  // small mouth
  paint.DrawFilledCircle(125, 96, 4, B);
  paint.DrawFilledCircle(125, 94, 4, W);   // erase top → smile arc

  // label at bottom
  const char* expr;
  if      (eyesClosed) expr = "* blink *";
  else if (eyesHappy)  expr = "^_^  happy!";
  else if (pupilOX<0)  expr = "<  looking...";
  else if (pupilOX>0)  expr = "looking...  >";
  else                 expr = "Hi there!";
  paint.DrawStringAt(70, 110, expr, &Font12, B);
}

/* ─────────────────────────────────────────────────────────── */
/*                   SLEEP FACE SCREEN                         */
/* ─────────────────────────────────────────────────────────── */
void drawSleepFace() {
  const int LX = 72, RX = 178, EY = 50;
  const int R  = 28;

  // closed eyes
  drawSleepEye(LX, EY, R);
  drawSleepEye(RX, EY, R);

  // mouth — small "o"
  paint.DrawCircle(125, 80, 5, B);

  // floating "z Z Z" animation
  if (millis() - sleepTimer > 800) {
    sleepTimer = millis();
    sleepFrame = (sleepFrame + 1) % 3;
  }

  // three z's at increasing size, position shifts per frame
  int baseX = 190 + sleepFrame * 4;
  int baseY = 30  - sleepFrame * 3;
  paint.DrawStringAt(baseX,      baseY,      "z",  &Font12, B);
  paint.DrawStringAt(baseX + 12, baseY - 12, "Z",  &Font16, B);
  paint.DrawStringAt(baseX + 28, baseY - 28, "Z",  &Font20, B);

  paint.DrawStringAt(60, 105, "Shhh... sleeping", &Font12, B);
}

/* ─────────────────────────────────────────────────────────── */
/*                TEMPERATURE & CALENDAR SCREEN                */
/* ─────────────────────────────────────────────────────────── */
void drawTempCalendar() {
  // ── dummy data ──
  const char* date  = "2026-02-21";
  const char* day   = "Saturday";
  const char* timeS = "14:30";
  int   tempInt     = 22;
  int   tempFrac    = 5;        // 22.5 °C
  int   humidity    = 55;       // %

  // ── title ──
  paint.DrawStringAt(6, 4, "Dashboard", &Font20, B);
  paint.DrawHorizontalLine(4, 26, 242, B);

  // ── date / day ──
  paint.DrawStringAt(6, 32, date, &Font16, B);
  paint.DrawStringAt(150, 34, day, &Font12, B);

  // ── large time ──
  paint.DrawStringAt(70, 52, timeS, &Font24, B);

  paint.DrawHorizontalLine(4, 82, 242, B);

  // ── temperature ──
  char buf[32];
  snprintf(buf, sizeof(buf), "Temp: %d.%d C", tempInt, tempFrac);
  paint.DrawStringAt(6, 88, buf, &Font16, B);

  // draw a tiny degree symbol (small circle)
  int tx = 6 + 15 * 7;  // approx position after "22.5"
  paint.DrawCircle(tx + 2, 90, 2, B);

  // ── humidity ──
  snprintf(buf, sizeof(buf), "Humidity: %d%%", humidity);
  paint.DrawStringAt(6, 108, buf, &Font12, B);

  // ── mini bar for temperature visual ──
  int barX = 170, barY = 90, barW = 70, barH = 12;
  paint.DrawRectangle(barX, barY, barX + barW, barY + barH, B);
  int fill = (int)(barW * (tempInt + tempFrac / 10.0) / 40.0); // 0-40°C scale
  if (fill > barW) fill = barW;
  paint.DrawFilledRectangle(barX, barY, barX + fill, barY + barH, B);
}

/* ─────────────────────────────────────────────────────────── */
/*                   POMODORO TIMER SCREEN                     */
/* ─────────────────────────────────────────────────────────── */
void drawPomodoro() {
  // ── timer logic ──
  uint32_t elapsed = millis() - pomoStartMs;
  int32_t  remain  = (int32_t)pomoDurationMs - (int32_t)elapsed;

  if (remain <= 0) {
    // timer expired → toggle focus / break
    pomoIsBreak   = !pomoIsBreak;
    pomoDurationMs = pomoIsBreak ? 5UL * 60 * 1000 : 25UL * 60 * 1000;
    pomoStartMs    = millis();
    if (!pomoIsBreak) pomoSession++;
    remain = pomoDurationMs;
  }

  uint32_t secTotal = remain / 1000;
  int mn = secTotal / 60;
  int sc = secTotal % 60;

  // ── header ──
  const char* label = pomoIsBreak ? "BREAK" : "FOCUS";
  paint.DrawStringAt(80, 4, label, &Font24, B);
  paint.DrawHorizontalLine(4, 30, 242, B);

  // ── large countdown ──
  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", mn, sc);
  paint.DrawStringAt(60, 38, timeBuf, &Font24, B);

  // ── progress bar ──
  float progress = (float)elapsed / pomoDurationMs;
  if (progress > 1.0f) progress = 1.0f;
  int barX = 20, barY = 72, barW = 210, barH = 16;
  paint.DrawRectangle(barX, barY, barX + barW, barY + barH, B);
  int fillW = (int)(barW * progress);
  if (fillW > 0)
    paint.DrawFilledRectangle(barX + 1, barY + 1,
                              barX + fillW, barY + barH - 1, B);

  // percentage
  char pctBuf[8];
  snprintf(pctBuf, sizeof(pctBuf), "%d%%", (int)(progress * 100));
  paint.DrawStringAt(110, 92, pctBuf, &Font12, B);

  // ── session indicator ──
  char sessBuf[24];
  snprintf(sessBuf, sizeof(sessBuf), "Session %d", pomoSession);
  paint.DrawStringAt(80, 108, sessBuf, &Font12, B);

  // 4-dot session tracker
  for (int i = 0; i < 4; i++) {
    int dx = 20 + i * 18;
    if (i < pomoSession)
      paint.DrawFilledCircle(dx, 112, 4, B);
    else
      paint.DrawCircle(dx, 112, 4, B);
  }
}

/* ─────────────────────────────────────────────────────────── */
/*                    REFRESH DISPLAY                          */
/* ─────────────────────────────────────────────────────────── */
void refreshDisplay() {
  if (curMode == MODE_FACEDOWN) return;      // don't touch display

  // choose full or partial refresh
  bool doFull = forceFullRefresh || (partialCount >= PARTIAL_LIMIT);
  if (doFull) epd.Init(FULL); else epd.Init(PART);

  paint.Clear(W);

  // render current mode
  switch (curMode) {
    case MODE_PET:       drawPetFace();      break;
    case MODE_SLEEP:     drawSleepFace();    break;
    case MODE_TEMPTIME:  drawTempCalendar(); break;
    case MODE_POMODORO:  drawPomodoro();     break;
    default: break;
  }

  // debug: tiny roll/pitch readout at bottom-right corner
  {
    char dbg[24];
    int ri = (int)rollDeg;
    int pi = (int)pitchDeg;
    snprintf(dbg, sizeof(dbg), "R%d P%d", ri, pi);
    paint.DrawStringAt(195, 0, dbg, &Font8, B);
  }

  if (doFull) {
    epd.DisplayPartBaseImage(framebuf);
    partialCount   = 0;
    forceFullRefresh = false;
  } else {
    epd.DisplayPart(framebuf);
    partialCount++;
  }
}

/* ─────────────────────────────────────────────────────────── */
/*                        LOOP                                 */
/* ─────────────────────────────────────────────────────────── */
void loop() {
  detectMode();

  // serial debug
  Serial.print(F("R:"));  Serial.print(rollDeg, 1);
  Serial.print(F(" P:")); Serial.print(pitchDeg, 1);
  Serial.print(F(" M:")); Serial.println(curMode);

  refreshDisplay();

  delay(500);
}
