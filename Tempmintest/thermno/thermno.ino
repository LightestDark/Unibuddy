/*
 * ============================================================
 *  Modulino Thermo Test — Output on Waveshare 2.13" V2 e-Paper
 *
 *  Thermo Module: Qwiic connection (I2C)
 *  E-Paper:       Waveshare 2.13" V2
 *                 VCC→5V, GND→GND, DIN→D11, CLK→D13,
 *                 CS→D10, DC→D9, RST→D8, BUSY→D7
 *
 *  Library needed:
 *    - "Modulino" by Arduino  (Library Manager)
 *
 *  Also needs Waveshare's own files in the same folder:
 *    - epd2in13_V2.h / epd2in13_V2.cpp
 *    - epdpaint.h / epdpaint.cpp
 *    - fonts.h
 *  (These come from the Waveshare demo zip you downloaded)
 * ============================================================
 */

#include <SPI.h>
#include <Wire.h>
#include <Modulino.h>
#include "epd2in13_V4.h"
#include "epdpaint.h"

// ── E-Paper setup ─────────────────────────────────────────────
Epd epd;
unsigned char framebuf[EPD_WIDTH / 8 * EPD_HEIGHT];
Paint paint(framebuf, EPD_WIDTH, EPD_HEIGHT);

#define COL_BLACK 0
#define COL_WHITE 1

// ── Modulino Thermo ───────────────────────────────────────────
ModulinoThermo thermo;
bool thermoOK = false;

// ── Screen update timing ──────────────────────────────────────
uint32_t lastScreenMs   = 0;
uint32_t lastReadMs     = 0;
#define READ_INTERVAL   2000   // read sensor every 2 seconds
#define SCREEN_INTERVAL 3000   // refresh screen every 3 seconds

// ── Stored readings ───────────────────────────────────────────
float currentTemp     = 0.0;
float currentHumidity = 0.0;
uint32_t readCount    = 0;
char statusMsg[32]    = "Starting...";

// ── Draw the screen ───────────────────────────────────────────
void drawScreen() {
  paint.SetRotate(ROTATE_270);   // landscape
  paint.Clear(COL_WHITE);

  char buf[40];

  // Title
  paint.DrawStringAt(4, 2, "MODULINO THERMO", &Font16, COL_BLACK);
  paint.DrawHorizontalLine(4, 20, 242, COL_BLACK);

  if (!thermoOK) {
    // Error state
    paint.DrawStringAt(4, 30, "ERROR:", &Font16, COL_BLACK);
    paint.DrawStringAt(4, 50, "Thermo not found!", &Font12, COL_BLACK);
    paint.DrawStringAt(4, 66, "Check Qwiic cable.", &Font12, COL_BLACK);
    return;
  }

  // Temperature — big and prominent
  paint.DrawStringAt(4, 28, "TEMPERATURE", &Font12, COL_BLACK);
  snprintf(buf, sizeof(buf), "%.1f C", currentTemp);
  paint.DrawStringAt(4, 44, buf, &Font24, COL_BLACK);

  // Humidity
  paint.DrawStringAt(4, 74, "HUMIDITY", &Font12, COL_BLACK);
  snprintf(buf, sizeof(buf), "%.1f %%", currentHumidity);
  paint.DrawStringAt(4, 90, buf, &Font16, COL_BLACK);

  // Divider + footer
  paint.DrawHorizontalLine(4, 110, 242, COL_BLACK);

  snprintf(buf, sizeof(buf), "Reads: %lu  | %s",
           (unsigned long)readCount, statusMsg);
  paint.DrawStringAt(4, 114, buf, &Font12, COL_BLACK);
}

void refreshScreen() {
  drawScreen();
  epd.Init(PART);
  epd.DisplayPart(framebuf);
  lastScreenMs = millis();
}

// ── Setup ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println(F("[Thermo] Starting..."));

  // Init Modulino / I2C
  Modulino.begin();

  if (!thermo.begin()) {
    Serial.println(F("[Thermo] ERROR: sensor not found! Check Qwiic cable."));
    thermoOK = false;
    snprintf(statusMsg, sizeof(statusMsg), "NOT FOUND");
  } else {
    Serial.println(F("[Thermo] Sensor found!"));
    thermoOK = true;
    snprintf(statusMsg, sizeof(statusMsg), "OK");
  }

  // Init e-paper full refresh
  if (epd.Init(FULL) != 0) {
    Serial.println(F("[Thermo] EPD init FAILED! Check wiring."));
    while (true);
  }
  epd.Clear();
  Serial.println(F("[Thermo] EPD ready."));

  // Draw base image (full refresh first)
  drawScreen();
  epd.DisplayPartBaseImage(framebuf);

  Serial.println(F("[Thermo] Ready! Reading every 2 seconds."));
}

// ── Main loop ─────────────────────────────────────────────────
void loop() {
  uint32_t now = millis();

  // Read sensor every READ_INTERVAL ms
  if (thermoOK && (now - lastReadMs >= READ_INTERVAL)) {
    lastReadMs = now;

    currentTemp     = thermo.getTemperature();
    currentHumidity = thermo.getHumidity();
    readCount++;

    snprintf(statusMsg, sizeof(statusMsg), "OK");

    Serial.print(F("[Thermo] Temp: "));
    Serial.print(currentTemp);
    Serial.print(F(" C  |  Humidity: "));
    Serial.print(currentHumidity);
    Serial.println(F(" %"));
  }

  // Refresh screen every SCREEN_INTERVAL ms
  if (now - lastScreenMs >= SCREEN_INTERVAL) {
    refreshScreen();
  }

  delay(50);
}

