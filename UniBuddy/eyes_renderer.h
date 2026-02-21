#pragma once

#include <Arduino.h>
#include "epdpaint.h"
#include "eyes.h"

inline void drawThickLine(Paint& paint, int x0, int y0, int x1, int y1, int color) {
  paint.DrawLine(x0, y0, x1, y1, color);
  paint.DrawLine(x0, y0 + 1, x1, y1 + 1, color);
}

inline void drawSingleEmotionEye(Paint& paint, int x, int y, int w, int h, const EyeFrame& frame, bool leftEye, int color) {
  int cx = x + w / 2;
  int cy = y + h / 2;
  int r = (h < w ? h : w) / 2 - 2;

  paint.DrawCircle(cx, cy, r, color);
  paint.DrawCircle(cx, cy, r - 1, color);

  if (frame.lineStyle == EYE_LINE_FLAT) {
    drawThickLine(paint, x + 6, cy, x + w - 6, cy, color);
    return;
  }

  if (frame.lineStyle == EYE_LINE_DIAG_UP) {
    if (leftEye) drawThickLine(paint, x + 6, cy + 4, x + w - 6, cy - 3, color);
    else         drawThickLine(paint, x + 6, cy - 3, x + w - 6, cy + 4, color);
    return;
  }

  if (frame.lineStyle == EYE_LINE_DIAG_DOWN) {
    if (leftEye) drawThickLine(paint, x + 6, cy - 3, x + w - 6, cy + 4, color);
    else         drawThickLine(paint, x + 6, cy + 4, x + w - 6, cy - 3, color);
    return;
  }

  if (frame.lineStyle == EYE_LINE_KYAAA) {
    if (leftEye) {
      drawThickLine(paint, x + 8, cy - 2, x + w - 10, cy - 8, color);
      drawThickLine(paint, x + 8, cy + 2, x + w - 10, cy + 8, color);
    } else {
      drawThickLine(paint, x + 10, cy - 8, x + w - 8, cy - 2, color);
      drawThickLine(paint, x + 10, cy + 8, x + w - 8, cy + 2, color);
    }
    return;
  }

  if (frame.upperLid > 0) {
    int yLid = y + 5 + (int)frame.upperLid * 3;
    drawThickLine(paint, x + 5, yLid, x + w - 5, yLid, color);
  }
  if (frame.lowerLid > 0) {
    int yLid = y + h - 6 - (int)frame.lowerLid * 2;
    drawThickLine(paint, x + 6, yLid, x + w - 6, yLid, color);
  }

  if (frame.browTilt != 0) {
    int browY = y + 2;
    int browEndY = browY + frame.browTilt;
    drawThickLine(paint, x + 6, browY, x + w - 6, browEndY, color);
  }

  if (frame.drawPupil) {
    paint.DrawFilledCircle(cx + frame.pupilX, cy + frame.pupilY, frame.pupilR, color);
  }
}

inline void drawEyeEmotionAccents(Paint& paint, int originX, int originY, int eyeW, int eyeGap, EyeEmotion emotion, uint8_t phase, int color) {
  EyeAccentFlags flags = getEyeEmotionAccents(emotion, phase);
  int rightEyeX = originX + eyeW + eyeGap;

  if (flags.heart) {
    paint.DrawFilledCircle(rightEyeX + eyeW - 8, originY - 2, 2, color);
    paint.DrawFilledCircle(rightEyeX + eyeW - 4, originY - 2, 2, color);
    paint.DrawLine(rightEyeX + eyeW - 10, originY, rightEyeX + eyeW - 6, originY + 5, color);
    paint.DrawLine(rightEyeX + eyeW - 2, originY, rightEyeX + eyeW - 6, originY + 5, color);
  }
  if (flags.tear) {
    paint.DrawFilledCircle(rightEyeX + eyeW - 4, originY + 28, 2, color);
  }
  if (flags.sweat) {
    paint.DrawLine(rightEyeX + eyeW - 10, originY - 2, rightEyeX + eyeW - 6, originY + 4, color);
    paint.DrawLine(rightEyeX + eyeW - 9, originY - 1, rightEyeX + eyeW - 5, originY + 5, color);
  }
  if (flags.zzz) {
    paint.DrawStringAt(rightEyeX + eyeW - 10, originY - 8, "z", &Font12, color);
    paint.DrawStringAt(rightEyeX + eyeW - 2, originY - 13, "z", &Font12, color);
  }
}

inline void drawEyePairEmotion(Paint& paint, int originX, int originY, int eyeW, int eyeH, int eyeGap, EyeEmotion emotion, uint8_t phase, int color) {
  EyeFrame leftFrame = getEyeFrame(emotion, phase, true);
  EyeFrame rightFrame = getEyeFrame(emotion, phase, false);
  drawSingleEmotionEye(paint, originX, originY, eyeW, eyeH, leftFrame, true, color);
  drawSingleEmotionEye(paint, originX + eyeW + eyeGap, originY, eyeW, eyeH, rightFrame, false, color);
  drawEyeEmotionAccents(paint, originX, originY, eyeW, eyeGap, emotion, phase, color);
}
