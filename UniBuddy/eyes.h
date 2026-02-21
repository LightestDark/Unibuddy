#pragma once

#include <Arduino.h>

enum EyeEmotion : uint8_t {
  EYE_NEUTRAL,
  EYE_LOOK_LEFT,
  EYE_LOOK_RIGHT,
  EYE_HAPPY,
  EYE_SAD,
  EYE_ANNOYED,
  EYE_BORED,
  EYE_TIRED,
  EYE_CLOSED,
  EYE_SURPRISED,
  EYE_KYAAA,
  EYE_WORRIED,
  EYE_SLEEP
};

enum EyeLineStyle : uint8_t {
  EYE_LINE_OPEN,
  EYE_LINE_FLAT,
  EYE_LINE_DIAG_UP,
  EYE_LINE_DIAG_DOWN,
  EYE_LINE_KYAAA
};

struct EyeFrame {
  int8_t pupilX;
  int8_t pupilY;
  uint8_t pupilR;
  uint8_t upperLid;
  uint8_t lowerLid;
  int8_t browTilt;
  bool drawPupil;
  EyeLineStyle lineStyle;
};

struct EyeAccentFlags {
  bool heart;
  bool tear;
  bool sweat;
  bool zzz;
};

static EyeEmotion _eyeEmotion = EYE_NEUTRAL;
static uint8_t _eyeAnimPhase = 0;
static uint32_t _lastEyeAnimTick = 0;

uint8_t getEyeEmotionCount() {
  return 13;
}

EyeEmotion getEyeEmotionByIndex(uint8_t index) {
  return (EyeEmotion)(index % getEyeEmotionCount());
}

const char* getEyeEmotionName(EyeEmotion emotion) {
  switch (emotion) {
    case EYE_NEUTRAL:   return "NEUTRAL";
    case EYE_LOOK_LEFT: return "LEFT";
    case EYE_LOOK_RIGHT:return "RIGHT";
    case EYE_HAPPY:     return "HAPPY";
    case EYE_SAD:       return "SAD";
    case EYE_ANNOYED:   return "ANNOYED";
    case EYE_BORED:     return "BORED";
    case EYE_TIRED:     return "TIRED";
    case EYE_CLOSED:    return "CLOSED";
    case EYE_SURPRISED: return "SURPRISED";
    case EYE_KYAAA:     return "KYAAA";
    case EYE_WORRIED:   return "WORRIED";
    case EYE_SLEEP:
    default:            return "SLEEP";
  }
}

uint8_t getEyeEmotionPhaseCount(EyeEmotion emotion) {
  switch (emotion) {
    case EYE_NEUTRAL:  return 3;   // center-left-right
    case EYE_HAPPY:    return 2;   // smile + blinky smile
    case EYE_SAD:      return 2;
    case EYE_ANNOYED:  return 2;
    case EYE_BORED:    return 2;
    case EYE_TIRED:    return 3;
    case EYE_SURPRISED:return 2;
    case EYE_WORRIED:  return 2;
    case EYE_SLEEP:    return 2;
    default:           return 1;
  }
}

uint16_t getEyeEmotionPhaseIntervalMs(EyeEmotion emotion, uint8_t phase) {
  switch (emotion) {
    case EYE_NEUTRAL:
      return (phase == 0) ? 2600 : 2200;
    case EYE_HAPPY:
      return 2400;
    case EYE_SAD:
      return 2500;
    case EYE_ANNOYED:
      return 2100;
    case EYE_BORED:
      return 2600;
    case EYE_TIRED:
      return (phase == 2) ? 3000 : 2400;
    case EYE_CLOSED:
      return 2600;
    case EYE_SURPRISED:
      return 2200;
    case EYE_KYAAA:
      return 2600;
    case EYE_WORRIED:
      return 2300;
    case EYE_SLEEP:
      return 2800;
    case EYE_LOOK_LEFT:
    case EYE_LOOK_RIGHT:
    default:
      return 2300;
  }
}

void setEyeEmotion(EyeEmotion emotion) {
  _eyeEmotion = emotion;
  _eyeAnimPhase = 0;
  _lastEyeAnimTick = millis();
}

EyeEmotion getEyeEmotion() {
  return _eyeEmotion;
}

uint8_t getEyeAnimPhase() {
  return _eyeAnimPhase;
}

bool tickEyeEmotionAnimation() {
  uint16_t interval = getEyeEmotionPhaseIntervalMs(_eyeEmotion, _eyeAnimPhase);
  if (millis() - _lastEyeAnimTick < interval) return false;
  _lastEyeAnimTick = millis();
  uint8_t count = getEyeEmotionPhaseCount(_eyeEmotion);
  _eyeAnimPhase = (uint8_t)((_eyeAnimPhase + 1) % count);
  return true;
}

EyeAccentFlags getEyeEmotionAccents(EyeEmotion emotion, uint8_t phase) {
  EyeAccentFlags flags = {false, false, false, false};
  switch (emotion) {
    case EYE_HAPPY:    flags.heart = (phase == 0); break;
    case EYE_SAD:      flags.tear = (phase == 1); break;
    case EYE_TIRED:
    case EYE_WORRIED:  flags.sweat = (phase == 1); break;
    case EYE_SLEEP:    flags.zzz = true; break;
    default: break;
  }
  return flags;
}

EyeFrame getEyeFrame(EyeEmotion emotion, uint8_t phase, bool leftEye) {
  EyeFrame frame;
  frame.pupilX = 0;
  frame.pupilY = 0;
  frame.pupilR = 5;
  frame.upperLid = 0;
  frame.lowerLid = 0;
  frame.browTilt = 0;
  frame.drawPupil = true;
  frame.lineStyle = EYE_LINE_OPEN;

  switch (emotion) {
    case EYE_NEUTRAL:
      if (phase == 1) frame.pupilX = -8;
      else if (phase == 2) frame.pupilX = 8;
      break;

    case EYE_LOOK_LEFT:
      frame.pupilX = -9;
      break;

    case EYE_LOOK_RIGHT:
      frame.pupilX = 9;
      break;

    case EYE_HAPPY:
      frame.upperLid = 1;
      frame.lowerLid = 2;
      frame.pupilY = 3;
      if (phase == 1) {
        frame.drawPupil = false;
        frame.lineStyle = EYE_LINE_FLAT;
      }
      break;

    case EYE_SAD:
      frame.upperLid = 1;
      frame.lowerLid = 2;
      frame.pupilY = 4;
      frame.browTilt = leftEye ? -3 : 3;
      break;

    case EYE_ANNOYED:
      frame.upperLid = 2;
      frame.browTilt = leftEye ? 5 : -5;
      frame.pupilY = 2;
      if (phase == 1) frame.pupilX = leftEye ? -3 : 3;
      break;

    case EYE_BORED:
      frame.upperLid = 2;
      frame.lowerLid = 1;
      frame.pupilY = 4;
      frame.pupilR = 4;
      if (phase == 1) frame.drawPupil = false;
      break;

    case EYE_TIRED:
      frame.upperLid = 3;
      frame.lowerLid = 1;
      frame.pupilY = 5;
      frame.pupilR = 4;
      if (phase == 1) {
        frame.drawPupil = false;
        frame.lineStyle = EYE_LINE_DIAG_DOWN;
      } else if (phase == 2) {
        frame.drawPupil = false;
        frame.lineStyle = EYE_LINE_FLAT;
      }
      break;

    case EYE_CLOSED:
      frame.drawPupil = false;
      frame.lineStyle = EYE_LINE_FLAT;
      break;

    case EYE_SURPRISED:
      frame.pupilR = 2;
      if (phase == 1) {
        frame.pupilX = leftEye ? -2 : 2;
        frame.pupilY = -1;
      }
      break;

    case EYE_KYAAA:
      frame.drawPupil = false;
      frame.lineStyle = EYE_LINE_KYAAA;
      break;

    case EYE_WORRIED:
      frame.pupilX = leftEye ? 2 : -2;
      frame.pupilY = 1;
      frame.browTilt = leftEye ? -2 : 2;
      if (phase == 1) frame.pupilY = 4;
      break;

    case EYE_SLEEP:
      frame.drawPupil = false;
      frame.lineStyle = (phase == 0) ? EYE_LINE_FLAT : EYE_LINE_DIAG_UP;
      break;
  }

  return frame;
}
