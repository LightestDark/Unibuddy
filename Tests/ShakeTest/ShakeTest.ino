/*
 * ============================================================
 *  ShakeTest.ino — Shake Detection Diagnostic
 *
 *  Serial + e-Paper output.
 *  Shows: raw vs filtered roll, magnitude bar,
 *         mode classification, shake counter, lockout status.
 *
 *  Upload to Arduino UNO R4 WiFi, open Serial @ 115200.
 * ============================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <math.h>
#include "Modulino.h"
#include "epd2in13_V4.h"
#include "epdpaint.h"

// ── Config ──────────────────────────────────────────────────
static const float EMA_ALPHA         = 0.12f;
static const float SHAKE_MAG_THRESH  = 1.8f;
static const uint16_t SHAKE_COOLDOWN_MS = 300;
static const uint16_t SHAKE_LOCKOUT_MS  = 800;

// Tilt thresholds (same as main project)
static const float TILT_ROLL_PET      = -70.0f;
static const float TILT_ROLL_POMO     =  70.0f;
static const float TILT_ROLL_SLEEP_LO = -25.0f;
static const float TILT_ROLL_SLEEP_HI =  25.0f;
static const float TILT_FACEDOWN_Z    = -0.5f;

// ── Display ─────────────────────────────────────────────────
Epd epd;
unsigned char fb[128 / 8 * 250];
Paint paint(fb, 128, 250);

static int partialCount   = 0;
static const int PARTIAL_LIMIT = 40;

// ── IMU State ───────────────────────────────────────────────
ModulinoMovement imu;

float rawX, rawY, rawZ;
float fX, fY, fZ;           // EMA-filtered
float roll_raw, roll_filt;
bool  emaInit = false;

float prevMag           = 1.0f;
uint32_t lastShakeEdge  = 0;
uint32_t shakeLockoutEnd = 0;

uint32_t shakeCount     = 0;
uint32_t lastPrint      = 0;
uint32_t lastDisplay    = 0;
float    peakMag        = 0.0f;    // peak magnitude since last display update

const char* classifyMode(float r, float az, bool reliable) {
  if (!reliable) return "LOCKED";
  if (az < TILT_FACEDOWN_Z)                                  return "FACEDOWN";
  if (r < TILT_ROLL_PET)                                     return "PET";
  if (r > TILT_ROLL_POMO)                                    return "POMO";
  if (r > TILT_ROLL_SLEEP_LO && r < TILT_ROLL_SLEEP_HI)     return "SLEEP";
  if (r <= TILT_ROLL_SLEEP_LO)                               return "TEMP_L";
  return "TEMP_R";
}

// ── ePaper helpers ──────────────────────────────────────────
void refreshFull() {
  epd.Init(FULL);
  epd.DisplayPartBaseImage(fb);
  partialCount = 0;
}

void refreshPartial() {
  if (partialCount >= PARTIAL_LIMIT) { refreshFull(); return; }
  epd.Init(PART);
  epd.DisplayPart(fb);
  partialCount++;
}

void drawDisplayFrame(float mag, bool reliable, bool shakeNow) {
  paint.Clear(1);  // white

  // ── Title ─────────────────────────────────────────────────
  paint.DrawStringAt(4, 2, "ShakeTest", &Font16, 0);
  paint.DrawHorizontalLine(2, 20, 246, 0);

  // ── Shake counter + status ────────────────────────────────
  {
    char buf[28];
    snprintf(buf, sizeof(buf), "Shakes: %lu", shakeCount);
    paint.DrawStringAt(4, 24, buf, &Font12, 0);
  }
  if (shakeNow) {
    paint.DrawStringAt(140, 24, ">>> SHAKE!", &Font12, 0);
  } else if (!reliable) {
    paint.DrawStringAt(140, 24, "[LOCKED]", &Font12, 0);
  } else {
    paint.DrawStringAt(140, 24, "OK", &Font12, 0);
  }

  // ── Magnitude bar ─────────────────────────────────────────
  paint.DrawStringAt(4, 40, "Mag:", &Font12, 0);
  {
    // bar: 0g..4g mapped to 0..180px
    int barX = 44, barY = 40, barW = 180, barH = 12;
    paint.DrawRectangle(barX, barY, barX + barW, barY + barH, 0);
    int fill = (int)(barW * mag / 4.0f);
    if (fill > barW) fill = barW;
    if (fill > 0) paint.DrawFilledRectangle(barX + 1, barY + 1, barX + fill, barY + barH - 1, 0);
    // threshold marker
    int thX = barX + (int)(barW * SHAKE_MAG_THRESH / 4.0f);
    paint.DrawVerticalLine(thX, barY - 2, barH + 4, 0);
    // value
    char mv[10];
    snprintf(mv, sizeof(mv), "%d.%02d", (int)mag, (int)(mag * 100) % 100);
    paint.DrawStringAt(barX + barW + 4, barY, mv, &Font12, 0);
  }

  // ── Peak magnitude ────────────────────────────────────────
  {
    char pk[20];
    snprintf(pk, sizeof(pk), "Peak: %d.%02d g", (int)peakMag, (int)(peakMag * 100) % 100);
    paint.DrawStringAt(4, 56, pk, &Font12, 0);
  }

  // ── Roll comparison ───────────────────────────────────────
  paint.DrawHorizontalLine(2, 70, 246, 0);
  {
    char r1[28], r2[28];
    snprintf(r1, sizeof(r1), "Roll raw:  %4d", (int)roll_raw);
    snprintf(r2, sizeof(r2), "Roll filt: %4d", (int)roll_filt);
    paint.DrawStringAt(4, 74, r1, &Font12, 0);
    paint.DrawStringAt(4, 88, r2, &Font12, 0);
    // delta
    int delta = (int)roll_raw - (int)roll_filt;
    char d[16];
    snprintf(d, sizeof(d), "d=%d", delta);
    paint.DrawStringAt(170, 74, d, &Font12, 0);
  }

  // ── Mode classification ───────────────────────────────────
  paint.DrawHorizontalLine(2, 102, 246, 0);
  {
    const char* modeRaw  = classifyMode(roll_raw,  rawZ, true);
    const char* modeFilt = classifyMode(roll_filt, fZ,   reliable);
    char m1[30], m2[30];
    snprintf(m1, sizeof(m1), "Raw:  %s", modeRaw);
    snprintf(m2, sizeof(m2), "Filt: %s", modeFilt);
    paint.DrawStringAt(4,   106, m1, &Font12, 0);
    paint.DrawStringAt(130, 106, m2, &Font12, 0);
  }
}

// ═════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n===== ShakeTest (ePaper) ====="));

  // Init display
  if (epd.Init(FULL) != 0) {
    Serial.println(F("[EPD] Init FAILED"));
  } else {
    epd.Clear();
    Serial.println(F("[EPD] Ready"));
  }
  paint.SetRotate(ROTATE_270);

  // Splash
  paint.Clear(1);
  paint.DrawStringAt(30, 20, "ShakeTest", &Font24, 0);
  paint.DrawStringAt(30, 52, "Initializing IMU...", &Font12, 0);
  epd.Display(fb);

  // Init IMU
  Modulino.begin();
  imu.begin();
  Serial.println(F("[IMU] Ready"));

  delay(1000);

  // First frame
  paint.Clear(1);
  paint.DrawStringAt(30, 50, "Ready! Shake me!", &Font16, 0);
  epd.Display(fb);
  delay(500);

  Serial.println(F("Columns: time | mag | roll_r | roll_f | mode_raw | mode_filt | status | shakes\n"));
}

void loop() {
  imu.update();
  rawX = imu.getX();
  rawY = imu.getY();
  rawZ = imu.getZ();

  // EMA filter
  if (!emaInit) {
    fX = rawX; fY = rawY; fZ = rawZ;
    emaInit = true;
  } else {
    fX += EMA_ALPHA * (rawX - fX);
    fY += EMA_ALPHA * (rawY - fY);
    fZ += EMA_ALPHA * (rawZ - fZ);
  }

  // Roll from raw vs filtered
  roll_raw  = atan2(rawY, rawZ) * 180.0f / M_PI;
  roll_filt = atan2(fY, fZ)     * 180.0f / M_PI;

  // Magnitude (raw)
  float mag = sqrt(rawX * rawX + rawY * rawY + rawZ * rawZ);
  if (mag > peakMag) peakMag = mag;

  // Shake detection
  uint32_t now = millis();
  bool shakeThisFrame = false;

  if (mag > SHAKE_MAG_THRESH && prevMag <= SHAKE_MAG_THRESH &&
      now - lastShakeEdge > SHAKE_COOLDOWN_MS) {
    shakeThisFrame   = true;
    lastShakeEdge    = now;
    shakeLockoutEnd  = now + SHAKE_LOCKOUT_MS;
    shakeCount++;
    Serial.print(F(">>> SHAKE #")); Serial.print(shakeCount);
    Serial.print(F("  mag="));
    Serial.println((int)(mag * 100));
  }
  prevMag = mag;

  bool reliable = (now >= shakeLockoutEnd);

  // ── Serial output (5 Hz) ─────────────────────────────────
  if (now - lastPrint >= 200) {
    lastPrint = now;

    const char* modeRaw  = classifyMode(roll_raw,  rawZ, true);
    const char* modeFilt = classifyMode(roll_filt, fZ,  reliable);

    char buf[120];
    snprintf(buf, sizeof(buf),
      "%8lu | mag %5d | roll_r %4d | roll_f %4d | %7s | %7s | %s | shk %lu",
      now,
      (int)(mag * 100),
      (int)roll_raw,
      (int)roll_filt,
      modeRaw,
      modeFilt,
      reliable ? "OK    " : "LOCKED",
      shakeCount
    );
    Serial.println(buf);
  }

  // ── ePaper output (~2 Hz or on shake) ─────────────────────
  bool displayNow = (now - lastDisplay >= 500) || shakeThisFrame;
  if (displayNow) {
    lastDisplay = now;
    drawDisplayFrame(mag, reliable, shakeThisFrame);
    refreshPartial();
  }

  delay(10);  // ~100 Hz sample rate
}
