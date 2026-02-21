#pragma once
/*
 * ============================================================
 *  config.h — Central configuration for UniBuddy
 *  Tilt-based mode switching via Modulino Movement
 * ============================================================
 */

// Uncomment for rapid testing (10s focus / 3s break / 5s long break)
// #define TEST_MODE

// ── App modes (tilt-detected) ───────────────────────────────
enum AppMode {
  MODE_PET,            // standing upright  (roll < -70)   landscape ROTATE_270
  MODE_SLEEP,          // lying flat        (|roll| < 25)  landscape (adaptive)
  MODE_TEMPTIME_L,     // tilted left       (roll -70..-25) portrait ROTATE_0
  MODE_TEMPTIME_R,     // tilted right      (roll  25..70)  portrait ROTATE_180
  MODE_POMODORO,       // flipped upright   (roll > 70)    landscape ROTATE_90
  MODE_BREAK,          // (internal, same orientation as pomodoro)
  MODE_FACEDOWN        // accZ < -0.5  →  no display update
};

// ── Pin assignments ─────────────────────────────────────────
// E-Paper SPI: RST=D8 DC=D9 CS=D10 BUSY=D7 DIN=D11 CLK=D13
#define PIN_BUTTON        4
#define PIN_TAP_KY031     2
#define PIN_MOVEMENT      3
#define USE_SERVO_NUDGE   0

// ── Display ─────────────────────────────────────────────────
#define DISPLAY_WIDTH     250     // landscape
#define DISPLAY_HEIGHT    122
#define PORTRAIT_WIDTH    122     // portrait
#define PORTRAIT_HEIGHT   250

// ── Pomodoro durations (ms) ─────────────────────────────────
#ifdef TEST_MODE
  #define POMODORO_DURATION  (10UL * 1000)
  #define SHORT_BREAK        ( 3UL * 1000)
  #define LONG_BREAK         ( 5UL * 1000)
#else
  #define POMODORO_DURATION  (25UL * 60 * 1000)
  #define SHORT_BREAK        ( 5UL * 60 * 1000)
  #define LONG_BREAK         (15UL * 60 * 1000)
#endif

// ── Button timing (ms) ─────────────────────────────────────
#define BTN_DEBOUNCE_MS    50
#define BTN_LONG_PRESS_MS  600

// ── Tilt detection thresholds ───────────────────────────────
#define TILT_ROLL_PET         -70.0f   //  < -70 = PET
#define TILT_ROLL_POMO         70.0f   //  >  70 = POMODORO
#define TILT_ROLL_SLEEP_LO    -35.0f   // -35..35 = SLEEP (wider)
#define TILT_ROLL_SLEEP_HI     35.0f
#define TILT_ROLL_CAL_LO      -65.0f   // -65..-35 = TEMPTIME_L (narrower)
#define TILT_ROLL_CAL_HI       65.0f   //  35..65 = TEMPTIME_R (narrower)
#define TILT_HYSTERESIS        10.0f   // extra margin to LEAVE current mode
#define TILT_FACEDOWN_Z       -0.5f
#define TILT_DEBOUNCE_COUNT    5

// ── Servo ───────────────────────────────────────────────────
#define SERVO_REST_ANGLE   0
#define SERVO_WAVE_ANGLE   90
#define SERVO_NUDGE_ANGLE  45

// ── Behaviour / EEPROM ──────────────────────────────────────
#define MAX_SESSIONS_STORED  20
