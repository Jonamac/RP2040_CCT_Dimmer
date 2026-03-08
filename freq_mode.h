// freq_mode.h
#pragma once
#include <Arduino.h>

// Number of discrete frequency steps
extern const int FREQ_STEPS;

// Frequency table in Hz, indexed 0..FREQ_STEPS-1
extern const float freqTable[];

// Optional helper: clamp index and return frequency
inline float getFreqForIndex(int idx) {
    if (idx < 0) idx = 0;
    if (idx >= FREQ_STEPS) idx = FREQ_STEPS - 1;
    return freqTable[idx];
}