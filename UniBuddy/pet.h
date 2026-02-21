#pragma once
/*
 * ============================================================
 *  pet.h — Virtual-pet mood & animation state
 *
 *  8-phase animation cycle:
 *    0 normal → 1 look-left → 2 normal → 3 blink
 *    4 normal → 5 look-right → 6 normal → 7 happy ^_^
 * ============================================================
 */
#include <Arduino.h>

// ── Pet emotions ────────────────────────────────────────────
enum PetMood {
  MOOD_HAPPY,
  MOOD_INTERESTED,
  MOOD_SAD,
  MOOD_ANGRY,
  MOOD_CONFUSED,
  MOOD_DESPISED,
  MOOD_FOCUSED,
  MOOD_TIRED,
  MOOD_ASLEEP,
};

// ── Animation ───────────────────────────────────────────────
static const uint8_t  PET_ANIM_PHASES = 8;
static const uint16_t PET_ANIM_MS[8]  = {
  2000,  // 0 normal
   800,  // 1 look left
  1500,  // 2 normal
   300,  // 3 blink (closed)
  2000,  // 4 normal
   800,  // 5 look right
  1500,  // 6 normal
  1200   // 7 happy ^_^
};

static PetMood  _mood          = MOOD_HAPPY;
static uint8_t  _animPhase     = 0;
static uint32_t _lastAnimTick  = 0;

// returns true when phase advances → signal a display refresh
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
    case MOOD_HAPPY:      return "HAPPY";
    case MOOD_INTERESTED: return "INTERESTED";
    case MOOD_SAD:        return "SAD";
    case MOOD_ANGRY:      return "ANGRY";
    case MOOD_CONFUSED:   return "CONFUSED";
    case MOOD_DESPISED:   return "DESPISED";
    case MOOD_TIRED:      return "TIRED";
    case MOOD_ASLEEP:     return "ASLEEP";
    case MOOD_FOCUSED:
    default:              return "FOCUSED";
  }
}

/* pupil horizontal offset: -8 left, +8 right, 0 center */
int8_t getPetEyeOffsetX() {
  if (_animPhase == 1) return -8;
  if (_animPhase == 5) return  8;
  return 0;
}

/* 0 = open, 2 = fully closed (blink) */
uint8_t getPetBlinkLevel() {
  return (_animPhase == 3) ? 2 : 0;
}

/* true during the happy ^_^ phase */
bool isPetHappyPhase() {
  return (_animPhase == 7);
}

void updatePetMoodFromSessions(uint8_t s) {
  if      (s == 0) setPetMood(MOOD_HAPPY);
  else if (s <= 1) setPetMood(MOOD_INTERESTED);
  else if (s <= 3) setPetMood(MOOD_FOCUSED);
  else if (s <= 5) setPetMood(MOOD_HAPPY);
  else if (s <= 7) setPetMood(MOOD_TIRED);
  else             setPetMood(MOOD_SAD);
}
