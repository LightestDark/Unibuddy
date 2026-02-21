#pragma once
/*
 * ============================================================
 *  pomodoro.h — Focus & break countdown timers
 *  Supports pause / resume so the user can tilt away briefly.
 * ============================================================
 */
#include <Arduino.h>
#include "config.h"

static uint32_t _pomStart      = 0;
static uint32_t _pomElapsed    = 0;       // accumulated ms when paused
static uint32_t _pomDuration   = POMODORO_DURATION;
static bool     _pomRunning    = false;
static bool     _pomFinished   = false;

static uint32_t _breakStart    = 0;
static uint32_t _breakDuration = SHORT_BREAK;
static bool     _breakFinished = false;

static uint8_t  _completedCycles = 0;

// ── Focus timer ─────────────────────────────────────────────
void initPomodoro() {
  _pomRunning  = false;
  _pomFinished = false;
  _pomElapsed  = 0;
}

void startPomodoro() {
  _pomStart    = millis();
  _pomElapsed  = 0;
  _pomRunning  = true;
  _pomFinished = false;
}

void pausePomodoro() {
  if (_pomRunning) {
    _pomElapsed += millis() - _pomStart;
    _pomRunning = false;
  }
}

void resumePomodoro() {
  if (!_pomRunning && !_pomFinished && _pomElapsed > 0) {
    _pomStart   = millis();
    _pomRunning = true;
  }
}

bool isPomRunning()  { return _pomRunning; }
bool isPomFinished() { return _pomFinished; }

void updatePomodoro() {
  if (!_pomRunning) return;
  uint32_t total = _pomElapsed + (millis() - _pomStart);
  if (total >= _pomDuration) {
    _pomRunning  = false;
    _pomFinished = true;
    _completedCycles++;
  }
}

bool isPomodoroFinished() {
  if (_pomFinished) { _pomFinished = false; return true; }
  return false;
}

uint32_t pomodoroSecondsLeft() {
  uint32_t total = _pomElapsed;
  if (_pomRunning) total += millis() - _pomStart;
  if (total >= _pomDuration) return 0;
  return (_pomDuration - total) / 1000;
}

// ── Break timer ─────────────────────────────────────────────
void startBreak() {
  _breakDuration = (_completedCycles % 4 == 0) ? LONG_BREAK : SHORT_BREAK;
  _breakStart    = millis();
  _breakFinished = false;
}

void tickBreakTimer() {
  if (millis() - _breakStart >= _breakDuration)
    _breakFinished = true;
}

bool isBreakFinished() {
  if (_breakFinished) { _breakFinished = false; return true; }
  return false;
}

uint32_t breakSecondsLeft() {
  uint32_t e = millis() - _breakStart;
  if (e >= _breakDuration) return 0;
  return (_breakDuration - e) / 1000;
}

// ── Getters ─────────────────────────────────────────────────
uint8_t getCompletedCycleCount() { return _completedCycles; }
