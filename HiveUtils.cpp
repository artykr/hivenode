#include "Arduino.h"
#include "HiveUtils.h"
#include "HiveSetup.h"
#define prog_char  char PROGMEM

unsigned long timeDiff(unsigned long timeValue) {
  unsigned long now = millis();

  if (now < timeValue) {
     return 0xffffffff - timeValue + now;
  } else {
    return now - timeValue;
  }
}

// http://forum.arduino.cc/index.php?topic=158375.0
void debugPrint(const __FlashStringHelper* pData, boolean newline) {
#ifdef HIVE_DEBUG

  char buffer[ 40 ]; //Size array as needed.
  int cursor = 0;
  prog_char *ptr = ( prog_char * ) pData;

  while( ( buffer[ cursor ] = pgm_read_byte_near( ptr + cursor ) ) != '\0' ) {
    ++cursor;
  }

  Serial.print( buffer );
  if (newline) {
    Serial.println("");
  }

#endif
}

void debugPrint(const char *pData, boolean newline) {
#ifdef HIVE_DEBUG

  Serial.print(pData);
  if (newline) {
    Serial.println("");
  }

#endif
}

void debugPrint(double pData, boolean newline) {
#ifdef HIVE_DEBUG

  Serial.print(pData);
  if (newline) {
    Serial.println("");
  }

#endif
}
