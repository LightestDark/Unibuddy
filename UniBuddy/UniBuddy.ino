/*
 * ============================================================
 *  UniBuddy.ino — Tilt-driven Pomodoro Pet Companion
 *
 *  Stand → Pet  |  Flat → Sleep  |  Tilt → Info
 *  Flip → Focus |  Face-down → Off
 *  Shake in Pet mode → mood reactions!
 * ============================================================
 */

#include "config.h"
#include "input.h"
#include "behaviour.h"
#include "tilt.h"
#include "epaper.h"      // includes pet.h, pomodoro.h internally

// ── State machine ───────────────────────────────────────────
static AppMode currentMode  = MODE_PET;
static AppMode prevMode     = MODE_PET;
static AppMode candidateMode = MODE_PET;
static uint8_t tiltCounter  = 0;

// ── Timing ──────────────────────────────────────────────────
static uint32_t _lastDisplayMs  = 0;
static const uint16_t DISPLAY_INTERVAL_MS = 300;
static bool _needsRedraw = true;

// ── Transition logic ────────────────────────────────────────

void transitionTo(AppMode newMode) {
  if (newMode == currentMode) return;

  Serial.print(F("[Mode] ")); Serial.print(currentMode);
  Serial.print(F(" -> "));    Serial.println(newMode);

  /* pause/resume pomodoro around focus mode */
  if (currentMode == MODE_POMODORO && isPomRunning())
    pausePomodoro();
  if (newMode == MODE_POMODORO && !isPomRunning() && !isPomFinished())
    resumePomodoro();

  prevMode    = currentMode;
  currentMode = newMode;

  /* rotation */
  int r = rotationForMode(currentMode, prevMode);
  setDisplayRotation(r);

  /* mode-enter actions */
  switch (currentMode) {
    case MODE_PET:
      setPetMood(MOOD_HAPPY);
      break;
    case MODE_POMODORO:
      setPetMood(MOOD_FOCUSED);
      if (!isPomRunning())
        startPomodoro();
      break;
    case MODE_SLEEP:
      setPetMood(MOOD_ASLEEP);
      break;
    default:
      break;
  }

  fullRefresh(currentMode);
  _needsRedraw = false;
}

// ═══════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("\n=== UniBuddy ==="));

  initInput();
  initTilt();
  initBehaviour();
  initPomodoro();
  initDisplay();
  showSplashScreen();
  delay(2000);

  /* first frame */
  int r = rotationForMode(MODE_PET, MODE_PET);
  setDisplayRotation(r);
  fullRefresh(MODE_PET);
}

// ═══════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════

void loop() {
  uint32_t now = millis();

  // ── 1. Read sensors ───────────────────────────────────────
  updateTilt();
  InputEvent evt = readInput();

  // ── 2. Shake handling (pet mode only) ─────────────────────
  if (wasShakeDetected() && currentMode == MODE_PET) {
    onShake();
    _needsRedraw = true;
  }

  // ── 3. Tilt-based mode switching (debounced + hysteresis + shake-lockout)
  if (isTiltReliable()) {
    AppMode tiltMode = classifyTilt(currentMode);

    /* keep pomodoro/break visually in same slot */
    if (currentMode == MODE_BREAK &&
        (tiltMode == MODE_POMODORO || tiltMode == MODE_BREAK))
      tiltMode = MODE_BREAK;

    if (tiltMode != currentMode) {
      if (tiltMode == candidateMode) {
        tiltCounter++;
      } else {
        candidateMode = tiltMode;
        tiltCounter   = 1;
      }
      if (tiltCounter >= TILT_DEBOUNCE_COUNT)
        transitionTo(tiltMode);
    } else {
      tiltCounter = 0;
    }
  } else {
    /* tilt unreliable (shake lockout) — reset debounce */
    tiltCounter = 0;
  }

  // ── 4. Button events ─────────────────────────────────────
  if (evt == EVT_BTN_SHORT) {
    if (currentMode == MODE_POMODORO && !isPomRunning()) {
      startPomodoro();
      _needsRedraw = true;
    }
  }
  if (evt == EVT_BTN_LONG) {
    /* long-press in pet → reset sessions */
    if (currentMode == MODE_PET) {
      initBehaviour();
      initPomodoro();
      setPetMood(MOOD_HAPPY);
      _needsRedraw = true;
      Serial.println(F("[Btn] Reset sessions"));
    }
  }

  // ── 5. Tap events ────────────────────────────────────────
  if (evt == EVT_TAP && currentMode == MODE_PET) {
    setPetMood(MOOD_INTERESTED);
    _needsRedraw = true;
  }
  if (evt == EVT_DOUBLE_TAP && currentMode == MODE_PET) {
    setPetMood(MOOD_HAPPY);
    _needsRedraw = true;
  }

  // ── 6. Pomodoro / break timers ────────────────────────────
  if (currentMode == MODE_POMODORO) {
    updatePomodoro();
    if (isPomodoroFinished()) {
      recordSession();
      startBreak();
      transitionTo(MODE_BREAK);
    }
  }
  if (currentMode == MODE_BREAK) {
    tickBreakTimer();
    if (isBreakFinished()) {
      updatePetMoodFromSessions(getSessionCount());
      transitionTo(MODE_PET);
    }
  }

  // ── 7. Pet idle mood decay ────────────────────────────────
  tickPetIdleMood();

  // ── 8. Pet animation tick ─────────────────────────────────
  if (currentMode == MODE_PET || currentMode == MODE_SLEEP) {
    if (tickPetAnimation())
      _needsRedraw = true;
  }

  // ── 9. Display refresh ───────────────────────────────────
  bool timerActive = (currentMode == MODE_POMODORO || currentMode == MODE_BREAK);

  if (timerActive || _needsRedraw) {
    if (now - _lastDisplayMs >= DISPLAY_INTERVAL_MS) {
      _lastDisplayMs = now;
      partialRefresh(currentMode);
      _needsRedraw = false;
    }
  }
}
