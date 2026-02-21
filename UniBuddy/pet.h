#pragma once
/*
 * ────────────────────────────────────────────────────────────
 *  pet.h — Virtual-pet mood & pixel-art sprites
 *
 *  Each sprite is a 16×16 monochrome bitmap (32 bytes, PROGMEM).
 *  The animation system toggles between two frames for blink.
 * ────────────────────────────────────────────────────────────
 */
#include <Arduino.h>
#include "eyes.h"

// ── Pet emotions ─────────────────────────────────────────────
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

// ── Internal state ──────────────────────────────────────────
static PetMood  _mood          = MOOD_HAPPY;

EyeEmotion mapMoodToEyeEmotion(PetMood mood) {
  switch (mood) {
    case MOOD_HAPPY:      return EYE_HAPPY;
    case MOOD_INTERESTED: return EYE_NEUTRAL;
    case MOOD_SAD:        return EYE_SAD;
    case MOOD_ANGRY:      return EYE_ANNOYED;
    case MOOD_CONFUSED:   return EYE_WORRIED;
    case MOOD_DESPISED:   return EYE_BORED;
    case MOOD_TIRED:      return EYE_TIRED;
    case MOOD_ASLEEP:     return EYE_SLEEP;
    case MOOD_FOCUSED:
    default:              return EYE_NEUTRAL;
  }
}

bool tickPetAnimation() {
  return tickEyeEmotionAnimation();
}

void setPetMood(PetMood mood) {
  _mood = mood;
  setEyeEmotion(mapMoodToEyeEmotion(mood));
}

PetMood getPetMood() { return _mood; }

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

uint8_t getPetAnimPhase() {
  return getEyeAnimPhase();
}

int8_t getPetEyeOffsetX() {
  EyeFrame left = getEyeFrame(getEyeEmotion(), getEyeAnimPhase(), true);
  return left.pupilX;
}

uint8_t getPetBlinkLevel() {
  EyeFrame left = getEyeFrame(getEyeEmotion(), getEyeAnimPhase(), true);
  if (left.lineStyle != EYE_LINE_OPEN) return 2;
  if (left.upperLid >= 2) return 1;
  return 0;
}

// Call after N sessions to update a default ambient emotion
void updatePetMoodFromSessions(uint8_t sessions) {
  if      (sessions == 0) setPetMood(MOOD_HAPPY);
  else if (sessions <= 1) setPetMood(MOOD_INTERESTED);
  else if (sessions <= 3) setPetMood(MOOD_FOCUSED);
  else if (sessions <= 5) setPetMood(MOOD_HAPPY);
  else if (sessions <= 7) setPetMood(MOOD_TIRED);
  else                    setPetMood(MOOD_SAD);
}
