/*
  HiveStorage.h - Utilities
*/

#ifndef HiveUtils_h
#define HiveUtils_h

#include "Arduino.h"
#include "HiveSetup.h"

unsigned long timeDiff(unsigned long timeValue);
void debugPrint(const __FlashStringHelper *pData, boolean newline = true);
void debugPrint(const char *pData, boolean newline = true);
void debugPrint(double pData, boolean newline = true);

#endif
