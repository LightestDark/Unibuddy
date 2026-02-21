/*
 * ============================================================
 *  UniBuddy.ino — Main sketch
 *  Tilt-based multi-screen Pomodoro companion
 *
 *  Board : Arduino UNO R4 WiFi
 *  IMU   : Modulino Movement (QWIIC)
 *  Display: Waveshare 2.13" e-Paper V4 (250×122)
 * ============================================================
 */

#include <SPI.h>
#include "config.h"
#include "tilt.h"
#include "epaper.h"
#include "pet.h"
#include "pomodoro.h"
#include "behaviour.h"
#include "input.h"

#if USE_SERVO_NUDGE
#include "servo_arm.h"
#endif

// ── Runtime state ───────────────────────────────────────────
AppMode  currentMode  = MODE_PET;
AppMode  prevMode     = MODE_PET;
int      curRotation  = ROTATE_270;

// tilt debounce
AppMode  pendingMode  = MODE_PET;
uint8_t  pendingCnt   = 0;

// refresh tracking
bool     forceRefresh   = true;
uint32_t lastTimerSec   = 0xFFFFFFFF;
uint32_t lastSleepAnim  = 0;

// ── Transition ──────────────────────────────────────────────
void transitionTo(AppMode newMode) {
  prevMode    = currentMode;
  currentMode = newMode;
  curRotation = rotationForMode(currentMode, prevMode);
  setDisplayRotation(curRotation);
  forceRefresh = true;

  Serial.print(F("[Mode] "));
  Serial.print(prevMode);
  Serial.print(F(" -> "));
  Serial.println(currentMode);

  /* entering pomodoro: start or resume timer */
  if (currentMode == MODE_POMODORO) {
    if (!isPomRunning()) {
      if (pomodoroSecondsLeft() > 0 && pomodoroSecondsLeft() < POMODORO_DURATION / 1000)
        resumePomodoro();        /* resume paused session */
      else
        startPomodoro();         /* fresh session */
      setPetMood(MOOD_FOCUSED);
    }
  }

  /* leaving pomodoro (not to break): pause timer */
  if (prevMode == MODE_POMODORO && currentMode != MODE_BREAK) {
    pausePomodoro();
  }

  /* mood hints */
  if (currentMode == MODE_PET)
    updatePetMoodFromSessions(getSessionCount());
  if (currentMode == MODE_SLEEP)
    setPetMood(MOOD_ASLEEP);
}

// ── Setup ───────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println(F("[UniBuddy] Tilt multi-screen"));

  initDisplay();
  initInput();
  initTilt();
  initPomodoro();
  initBehaviour();
#if USE_SERVO_NUDGE
  initServoArm();
#endif

  showSplashScreen();
  delay(2500);

  setDisplayRotation(ROTATE_270);
  fullRefresh(MODE_PET);
  Serial.println(F("[UniBuddy] Ready!"));
}

// ── Loop ────────────────────────────────────────────────────
void loop() {
  /* --- read sensors --- */
  updateTilt();
  InputEvent evt = readInput();

  /* --- classify tilt --- */
  AppMode tiltMode = classifyTilt();

  /* don't let tilt override internal break state */
  if (currentMode == MODE_BREAK && tiltMode == MODE_POMODORO)
    tiltMode = MODE_BREAK;

  /* --- debounce tilt transitions --- */
  if (tiltMode != currentMode) {
    if (tiltMode == pendingMode) {
      if (++pendingCnt >= TILT_DEBOUNCE_COUNT)
        transitionTo(tiltMode);
    } else {
      pendingMode = tiltMode;
      pendingCnt  = 1;
    }
  } else {
    pendingCnt = 0;
  }

  /* --- button shortcuts --- */
  if (evt == EVT_BTN_SHORT && currentMode == MODE_BREAK) {
    /* skip break */
    startPomodoro();
    setPetMood(MOOD_FOCUSED);
    currentMode = MODE_POMODORO;
    forceRefresh = true;
  }

  /* --- in-mode updates --- */
  switch (currentMode) {
    case MODE_POMODORO:
      updatePomodoro();
      if (isPomodoroFinished()) {
        recordSession();
        startBreak();
        prevMode    = MODE_POMODORO;
        currentMode = MODE_BREAK;
        setPetMood(MOOD_HAPPY);
        forceRefresh = true;
      }
      break;

    case MODE_BREAK:
      tickBreakTimer();
      if (isBreakFinished()) {
        /* if still tilted in pomo position → next session */
        AppMode tNow = classifyTilt();
        if (tNow == MODE_POMODORO) {
          startPomodoro();
          currentMode = MODE_POMODORO;
          setPetMood(MOOD_FOCUSED);
        } else {
          currentMode = tNow;
          curRotation = rotationForMode(currentMode, MODE_BREAK);
          setDisplayRotation(curRotation);
          updatePetMoodFromSessions(getSessionCount());
        }
        forceRefresh = true;
      }
      break;

    case MODE_FACEDOWN:
      /* display off, but keep timers running if active */
      if (isPomRunning()) updatePomodoro();
      break;

    default:
      break;
  }

  /* --- animation ticks --- */
  bool animTicked = tickPetAnimation();

  /* --- detect timer changes --- */
  bool timerTicked = false;
  if (currentMode == MODE_POMODORO) {
    uint32_t s = pomodoroSecondsLeft();
    if (s != lastTimerSec) { lastTimerSec = s; timerTicked = true; }
  } else if (currentMode == MODE_BREAK) {
    uint32_t s = breakSecondsLeft();
    if (s != lastTimerSec) { lastTimerSec = s; timerTicked = true; }
  }

  /* --- sleep zzz needs periodic refresh --- */
  bool sleepTicked = false;
  if (currentMode == MODE_SLEEP && millis() - lastSleepAnim > 800) {
    sleepTicked   = true;
    lastSleepAnim = millis();
  }

  /* --- display refresh --- */
  if (currentMode != MODE_FACEDOWN) {
    if (forceRefresh) {
      fullRefresh(currentMode);
      forceRefresh = false;
      lastTimerSec = 0xFFFFFFFF;
    } else if (timerTicked || animTicked || sleepTicked) {
      partialRefresh(currentMode);
    }
  }

  delay(80);
}
