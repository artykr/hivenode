#include "Arduino.h"
#include "HiveUtils.h"

unsigned long timeDiff(unsigned long timeValue) {
  unsigned long now = millis();
  
  if (now < timeValue) {
     return 0xffffffff - timeValue + now;
  } else {
    return now - timeValue;
  }
}