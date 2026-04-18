#ifndef BRIGHTNESS_TABLE_H
#define BRIGHTNESS_TABLE_H

#include <Arduino.h>

// Number of perceptual brightness steps
#define BRIGHTNESS_STEPS 101

// Perceptual brightness lookup table (0.0 → 1.0)
extern const float brightnessTable[BRIGHTNESS_STEPS];

#endif