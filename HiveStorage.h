/*
  HiveStorage.h - Abstraction class for storing data on a SD card
  with fallback to EEPROM
*/

#ifndef HiveStorage_h
#define HiveStorage_h

#include "Arduino.h"
#include "SD.h"
#include "EEPROM.h"
#include "HiveSetup.h"

extern uint8_t StorageType;

uint8_t initHiveStorage();
uint8_t initHiveSDStorage();
uint8_t initHiveEEPROMStorage();
    
template <class T> int writeStorage(int position, T& value) {
  const byte* p = (const byte*)(const void*)&value;
  unsigned int i;
    
  if (StorageType == EEPROMStorage) {
    for (i = 0; i < sizeof(value); i++)
      EEPROM.write(position++, *p++);
    return i;
  }
  
  if (StorageType == SDStorage) {
    File sdFile = SD.open(StorageFileName, FILE_WRITE);
    if (sdFile) {
      sdFile->seek(position);
      for (i = 0; i < sizeof(value); i++)
        sdFile->write(*p++);
      sdFile.close();
      return i;
    } else {
      return -1;
    }
  }
  
  return -1;
}
    
template <class T> int readStorage(int position, T& value) {
  byte* p = (byte*)(void*)&value;
  unsigned int i;
    
  if (StorageType == EEPROMStorage) {
    for (i = 0; i < sizeof(value); i++)
      *p++ = EEPROM.read(position++);
    return i;
  }
    
  if (StorageType == SDStorage) {
    File sdFile = SD.open(StorageFileName, FILE_READ);
    if (sdFile) {
      sdFile->seek(position);
      for (i = 0; i < sizeof(value); i++)
        *p++ = sdFile->read();
      sdFile.close();
      return i;
    }
  }
  
  return -1;   
}
    
#endif