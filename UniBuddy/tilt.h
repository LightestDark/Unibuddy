#pragma once
/*
 * ============================================================
 *  tilt.h — Modulino Movement tilt sensor interface
 *  Reads LSM6DSOX accelerometer via I²C/QWIIC, computes
 *  roll & pitch, classifies tilt into AppMode.
 * ============================================================
 */
#include <Arduino.h>
#include <math.h>
#include "Modulino.h"
#include "config.h"

static ModulinoMovement _imu;
static float _tAccX, _tAccY, _tAccZ;
static float _tRoll, _tPitch;

void initTilt() {
  Modulino.begin();
  _imu.begin();
  Serial.println(F("[Tilt] Modulino Movement ready"));
}

void updateTilt() {
  _imu.update();
  _tAccX = _imu.getX();
  _tAccY = _imu.getY();
  _tAccZ = _imu.getZ();
  _tRoll  = atan2(_tAccY, _tAccZ) * 180.0f / M_PI;
  _tPitch = atan2(-_tAccX, sqrt(_tAccY * _tAccY + _tAccZ * _tAccZ)) * 180.0f / M_PI;
}

float getRollDeg()  { return _tRoll; }
float getPitchDeg() { return _tPitch; }
float getAccZ()     { return _tAccZ; }

/* classify current tilt into an AppMode */
AppMode classifyTilt() {
  if (_tAccZ < TILT_FACEDOWN_Z)                            return MODE_FACEDOWN;
  if (_tRoll < TILT_ROLL_PET)                               return MODE_PET;
  if (_tRoll > TILT_ROLL_POMO)                              return MODE_POMODORO;
  if (_tRoll > TILT_ROLL_SLEEP_LO && _tRoll < TILT_ROLL_SLEEP_HI) return MODE_SLEEP;
  if (_tRoll <= TILT_ROLL_SLEEP_LO)                         return MODE_TEMPTIME_L;
  return MODE_TEMPTIME_R;
}

/* choose display rotation for a given mode, considering where the user was before */
int rotationForMode(AppMode mode, AppMode prev) {
  switch (mode) {
    case MODE_PET:        return ROTATE_270;
    case MODE_POMODORO:
    case MODE_BREAK:      return ROTATE_90;
    case MODE_TEMPTIME_L: return ROTATE_0;
    case MODE_TEMPTIME_R: return ROTATE_180;
    case MODE_SLEEP:
      /* adaptive: keep the orientation the user last saw */
      if (prev == MODE_POMODORO || prev == MODE_BREAK || prev == MODE_TEMPTIME_R)
        return ROTATE_90;
      return ROTATE_270;
    default:              return ROTATE_270;
  }
}
