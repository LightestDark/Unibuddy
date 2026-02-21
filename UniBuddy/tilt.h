#pragma once
/*
 * ============================================================
 *  tilt.h — Modulino Movement: tilt + shake detection
 *
 *  Robust 3-layer design:
 *    1. EMA low-pass filter on accel → stable tilt readings
 *    2. Shake detection via raw magnitude spike
 *    3. Shake lockout → tilt classification frozen for 800ms
 *       after a shake to prevent false mode switches
 * ============================================================
 */
#include <Arduino.h>
#include <math.h>
#include "Modulino.h"
#include "config.h"

#ifndef ROTATE_0
  #define ROTATE_0    0
  #define ROTATE_90   1
  #define ROTATE_180  2
  #define ROTATE_270  3
#endif

static ModulinoMovement _imu;

// ── Raw accelerometer (shake detection only) ────────────────
static float _rawX, _rawY, _rawZ;

// ── EMA-filtered accelerometer (tilt classification) ────────
static float _tAccX, _tAccY, _tAccZ;
static float _tRoll, _tPitch;
static bool  _emaInit = false;
static const float EMA_ALPHA = 0.12f;  // lower = smoother; ~12 samples to 90%

// ── Shake detection ─────────────────────────────────────────
static float    _prevMag          = 1.0f;
static bool     _shakeDetected    = false;
static uint32_t _lastShakeEdge    = 0;
static uint32_t _shakeLockoutEnd  = 0;

static const float    SHAKE_MAG_THRESH   = 1.8f;   // g magnitude spike
static const uint16_t SHAKE_COOLDOWN_MS  = 300;     // min gap between shakes
static const uint16_t SHAKE_LOCKOUT_MS   = 800;     // tilt frozen after shake

void initTilt() {
  Modulino.begin();
  _imu.begin();
  Serial.println(F("[Tilt] Modulino Movement ready"));
}

void updateTilt() {
  _imu.update();
  _rawX = _imu.getX();
  _rawY = _imu.getY();
  _rawZ = _imu.getZ();

  // ── EMA filter ────────────────────────────────────────────
  if (!_emaInit) {
    _tAccX = _rawX;  _tAccY = _rawY;  _tAccZ = _rawZ;
    _emaInit = true;
  } else {
    _tAccX += EMA_ALPHA * (_rawX - _tAccX);
    _tAccY += EMA_ALPHA * (_rawY - _tAccY);
    _tAccZ += EMA_ALPHA * (_rawZ - _tAccZ);
  }

  // ── Roll / pitch from FILTERED values ─────────────────────
  _tRoll  = atan2(_tAccY, _tAccZ) * 180.0f / M_PI;
  _tPitch = atan2(-_tAccX, sqrt(_tAccY * _tAccY + _tAccZ * _tAccZ))
            * 180.0f / M_PI;

  // ── Shake detection (from RAW magnitude) ──────────────────
  float mag = sqrt(_rawX * _rawX + _rawY * _rawY + _rawZ * _rawZ);
  uint32_t now = millis();
  _shakeDetected = false;

  if (mag > SHAKE_MAG_THRESH && _prevMag <= SHAKE_MAG_THRESH &&
      now - _lastShakeEdge > SHAKE_COOLDOWN_MS) {
    _shakeDetected   = true;
    _lastShakeEdge   = now;
    _shakeLockoutEnd = now + SHAKE_LOCKOUT_MS;
    Serial.print(F("[Tilt] SHAKE mag="));
    Serial.println(mag, 2);
  }
  _prevMag = mag;
}

// ── Getters ─────────────────────────────────────────────────
float getRollDeg()        { return _tRoll; }
float getPitchDeg()       { return _tPitch; }
float getAccZ()           { return _tAccZ; }   // filtered
bool  wasShakeDetected()  { return _shakeDetected; }

/*
 * isTiltReliable() — false during & shortly after shaking.
 * Callers MUST skip tilt-based mode switching when false.
 */
bool isTiltReliable() {
  return millis() >= _shakeLockoutEnd;
}

/* classify tilt with hysteresis — sticky current mode.
 *   • Leaving the current mode requires crossing threshold + HYSTERESIS.
 *   • Calendar zones are narrower and harder to enter.
 *   • Sleep zone is wider.
 */
AppMode classifyTilt(AppMode cur) {
  float r  = _tRoll;
  float az = _tAccZ;
  float H  = TILT_HYSTERESIS;   // 10° extra to leave current mode

  // face-down always takes priority
  if (az < TILT_FACEDOWN_Z)  return MODE_FACEDOWN;

  // --- PET (roll < -70) with hysteresis ---
  if (cur == MODE_PET) {
    // stay in PET until roll rises above PET + H = -60
    if (r < TILT_ROLL_PET + H) return MODE_PET;
  } else {
    if (r < TILT_ROLL_PET) return MODE_PET;
  }

  // --- POMODORO (roll > 70) with hysteresis ---
  if (cur == MODE_POMODORO || cur == MODE_BREAK) {
    // stay until roll drops below POMO - H = 60
    if (r > TILT_ROLL_POMO - H) return cur;
  } else {
    if (r > TILT_ROLL_POMO) return MODE_POMODORO;
  }

  // --- SLEEP (-35..35) with hysteresis ---
  if (cur == MODE_SLEEP) {
    // stay until roll leaves (-35-H .. 35+H) = (-45..45)
    if (r > TILT_ROLL_SLEEP_LO - H && r < TILT_ROLL_SLEEP_HI + H)
      return MODE_SLEEP;
  } else {
    if (r > TILT_ROLL_SLEEP_LO && r < TILT_ROLL_SLEEP_HI)
      return MODE_SLEEP;
  }

  // --- CALENDAR zones (narrower: -65..-35 and 35..65) ---
  if (cur == MODE_TEMPTIME_L) {
    // stay until roll leaves range + hysteresis
    if (r > TILT_ROLL_PET && r < TILT_ROLL_SLEEP_LO + H) return MODE_TEMPTIME_L;
  } else {
    if (r >= TILT_ROLL_CAL_LO && r <= TILT_ROLL_SLEEP_LO) return MODE_TEMPTIME_L;
  }

  if (cur == MODE_TEMPTIME_R) {
    if (r < TILT_ROLL_POMO && r > TILT_ROLL_SLEEP_HI - H) return MODE_TEMPTIME_R;
  } else {
    if (r <= TILT_ROLL_CAL_HI && r >= TILT_ROLL_SLEEP_HI) return MODE_TEMPTIME_R;
  }

  // fallback: stay in current mode if nothing else matched
  return cur;
}

/* choose display rotation for a given mode */
int rotationForMode(AppMode mode, AppMode prev) {
  switch (mode) {
    case MODE_PET:        return ROTATE_270;
    case MODE_POMODORO:
    case MODE_BREAK:      return ROTATE_90;
    case MODE_TEMPTIME_L: return ROTATE_0;
    case MODE_TEMPTIME_R: return ROTATE_180;
    case MODE_SLEEP:
      if (prev == MODE_POMODORO || prev == MODE_BREAK || prev == MODE_TEMPTIME_R)
        return ROTATE_90;
      return ROTATE_270;
    default:              return ROTATE_270;
  }
}
