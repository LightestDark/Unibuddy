#pragma once
/*
 * ============================================================
 *  pet.h — Virtual-pet mood, animation & shake-reaction system
 *
 *  Moods: happy, cute, interested, bored, surprised, worried,
 *         annoyed, dizzy, sad, angry, confused, focused,
 *         tired, asleep
 *
 *  Animation: 8-phase eye cycle (mood-aware).
 *  Shake system: gentle → amused → annoyed → dizzy.
 *  Idle decay: after long inactivity → bored.
 * ============================================================
 */
#include <Arduino.h>

// ── Moods ───────────────────────────────────────────────────
enum PetMood {
  MOOD_HAPPY,       // ^_^  bright eyes
  MOOD_CUTE,        // ⌒‿⌒ sparkle + blush
  MOOD_INTERESTED,  // big pupils, slight tilt
  MOOD_BORED,       // half-lid, looking away
  MOOD_SURPRISED,   // O_O  wide eyes
  MOOD_WORRIED,     // slanted brows, small pupils
  MOOD_ANNOYED,     // >_< frown
  MOOD_DIZZY,       // @_@ spiral eyes
  MOOD_SAD,         // droopy
  MOOD_ANGRY,       // v v brows
  MOOD_CONFUSED,    // ? one brow up
  MOOD_FOCUSED,     // squint
  MOOD_TIRED,       // droopy lids
  MOOD_ASLEEP,      // closed + zzz
};

// ── Animation ───────────────────────────────────────────────
//  0 normal → 1 look-L → 2 normal → 3 blink
//  4 normal → 5 look-R → 6 normal → 7 mood-special
static const uint8_t  PET_ANIM_PHASES = 8;
static const uint16_t PET_ANIM_MS[8]  = {
  2000, 800, 1500, 300, 2000, 800, 1500, 1200
};

static PetMood  _mood          = MOOD_HAPPY;
static uint8_t  _animPhase     = 0;
static uint32_t _lastAnimTick  = 0;

bool tickPetAnimation() {
  if (millis() - _lastAnimTick < PET_ANIM_MS[_animPhase]) return false;
  _lastAnimTick = millis();
  _animPhase = (_animPhase + 1) % PET_ANIM_PHASES;
  return true;
}

void setPetMood(PetMood m) {
  _mood = m;
  _animPhase = 0;
  _lastAnimTick = millis();
}

PetMood     getPetMood()      { return _mood; }
uint8_t     getPetAnimPhase() { return _animPhase; }

const char* getPetMoodName() {
  switch (_mood) {
    case MOOD_HAPPY:      return "happy";
    case MOOD_CUTE:       return "cute~";
    case MOOD_INTERESTED: return "interested";
    case MOOD_BORED:      return "bored...";
    case MOOD_SURPRISED:  return "!? surprised";
    case MOOD_WORRIED:    return "worried";
    case MOOD_ANNOYED:    return "annoyed >.<";
    case MOOD_DIZZY:      return "dizzy @_@";
    case MOOD_SAD:        return "sad";
    case MOOD_ANGRY:      return "angry!";
    case MOOD_CONFUSED:   return "confused?";
    case MOOD_FOCUSED:    return "focused";
    case MOOD_TIRED:      return "tired...";
    case MOOD_ASLEEP:     return "zzz";
    default:              return "...";
  }
}

/* pupil horizontal offset */
int8_t getPetEyeOffsetX() {
  if (_animPhase == 1) return -8;
  if (_animPhase == 5) return  8;
  return 0;
}

/* 0=open, 2=closed */
uint8_t getPetBlinkLevel() {
  return (_animPhase == 3) ? 2 : 0;
}

/* phase 7 = mood-specific special frame */
bool isPetSpecialPhase() {
  return (_animPhase == 7);
}

// ── Shake reaction system ───────────────────────────────────
static uint8_t  _shakesRecent   = 0;     // count in current window
static uint32_t _shakeWindowStart = 0;
static uint32_t _lastShakeTime  = 0;
static const uint32_t SHAKE_WINDOW_MS     = 15000;   // 15s rolling window
static const uint32_t SHAKE_DECAY_MS      = 30000;   // fully calm after 30s idle
static const uint8_t  SHAKE_AMUSED_THRESH = 2;
static const uint8_t  SHAKE_ANNOYED_THRESH = 5;
static const uint8_t  SHAKE_DIZZY_THRESH  = 8;

/* called each time a shake is detected */
void onShake() {
  uint32_t now = millis();
  if (now - _shakeWindowStart > SHAKE_WINDOW_MS) {
    _shakesRecent     = 0;
    _shakeWindowStart = now;
  }
  _shakesRecent++;
  _lastShakeTime = now;

  if (_shakesRecent >= SHAKE_DIZZY_THRESH)
    setPetMood(MOOD_DIZZY);
  else if (_shakesRecent >= SHAKE_ANNOYED_THRESH)
    setPetMood(MOOD_ANNOYED);
  else if (_shakesRecent >= SHAKE_AMUSED_THRESH)
    setPetMood(MOOD_SURPRISED);
  else
    setPetMood(MOOD_CUTE);    // gentle shake → cute
}

/* call every loop iteration to handle mood decay over time */
void tickPetIdleMood() {
  uint32_t now = millis();
  /* reset shake window */
  if (_shakesRecent > 0 && now - _shakeWindowStart > SHAKE_WINDOW_MS)
    _shakesRecent = 0;

  /* after shaking: gradually calm down */
  if (_lastShakeTime > 0 && now - _lastShakeTime > SHAKE_DECAY_MS) {
    if (_mood == MOOD_DIZZY || _mood == MOOD_ANNOYED ||
        _mood == MOOD_SURPRISED || _mood == MOOD_CUTE) {
      setPetMood(MOOD_HAPPY);
      _lastShakeTime = 0;
    }
  }

  /* long idle → bored (only if currently happy/interested) */
  if (_lastShakeTime == 0 &&
      (_mood == MOOD_HAPPY || _mood == MOOD_INTERESTED) &&
      now - _lastAnimTick > 60000) {
    setPetMood(MOOD_BORED);
  }
}

/* session-based ambient mood */
void updatePetMoodFromSessions(uint8_t s) {
  if      (s == 0)  setPetMood(MOOD_HAPPY);
  else if (s == 1)  setPetMood(MOOD_INTERESTED);
  else if (s <= 3)  setPetMood(MOOD_CUTE);
  else if (s <= 5)  setPetMood(MOOD_HAPPY);
  else if (s <= 7)  setPetMood(MOOD_TIRED);
  else              setPetMood(MOOD_SAD);
}
