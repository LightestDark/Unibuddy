/*
 * ============================================================
 *  Eyes Carousel Test - UniBuddy emotion preview on e-paper
 *  Rotates through all eye emotions as an image carousel.
 * ============================================================
 */

#include <SPI.h>
#include "../../UniBuddy/epd2in13_V4.h"
#include "../../UniBuddy/epdpaint.h"
#include "../../UniBuddy/eyes.h"
#include "../../UniBuddy/eyes_renderer.h"

#define COL_BLACK  0
#define COL_WHITE  1

static unsigned char framebuf[128 / 8 * 250];
static Paint paint(framebuf, 128, 250);
static Epd epd;

static uint8_t emotionIndex = 0;
static uint8_t completedCycles = 0;

void renderEmotionFrame() {
  paint.Clear(COL_WHITE);

  paint.DrawStringAt(10, 6, "EYES CAROUSEL", &Font16, COL_BLACK);
  paint.DrawHorizontalLine(8, 25, 234, COL_BLACK);

  EyeEmotion emotion = getEyeEmotion();
  uint8_t phase = getEyeAnimPhase();

  char label[42];
  snprintf(label, sizeof(label), "%s  phase:%d", getEyeEmotionName(emotion), phase);
  paint.DrawStringAt(10, 30, label, &Font12, COL_BLACK);

  char idx[32];
  snprintf(idx, sizeof(idx), "%d / %d", emotionIndex + 1, getEyeEmotionCount());
  paint.DrawStringAt(190, 30, idx, &Font12, COL_BLACK);

  drawEyePairEmotion(paint, 38, 50, 74, 52, 24, emotion, phase, COL_BLACK);
  paint.DrawRectangle(0, 0, 249, 121, COL_BLACK);
}

void fullRender() {
  epd.Init(FULL);
  renderEmotionFrame();
  epd.DisplayPartBaseImage(framebuf);
}

void partialRender() {
  epd.Init(PART);
  renderEmotionFrame();
  epd.DisplayPart(framebuf);
}

void nextEmotion() {
  emotionIndex = (uint8_t)((emotionIndex + 1) % getEyeEmotionCount());
  setEyeEmotion(getEyeEmotionByIndex(emotionIndex));
  completedCycles = 0;
  fullRender();

  Serial.print(F("[Carousel] Emotion -> "));
  Serial.println(getEyeEmotionName(getEyeEmotion()));
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("[Carousel] Init"));

  if (epd.Init(FULL) != 0) {
    Serial.println(F("[Carousel] EPD init failed"));
    while (true);
  }
  epd.Clear();
  paint.SetRotate(ROTATE_270);

  setEyeEmotion(getEyeEmotionByIndex(emotionIndex));
  fullRender();
  Serial.println(F("[Carousel] Running"));
}

void loop() {
  if (tickEyeEmotionAnimation()) {
    partialRender();

    if (getEyeAnimPhase() == 0) {
      completedCycles++;
      if (completedCycles >= 1) {
        nextEmotion();
      }
    }
  }

  delay(50);
}
