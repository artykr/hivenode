/*
  HiveStorage.h - Abstraction class for storing data on a SD card
  with fallback to EEPROM
*/

#ifndef HiveStorage_h
#define HiveStorage_h

#include "EEPROM.h"
#include "SD.h"
#include "SPI.h"
#include "HiveSetup.h"

extern uint8_t StorageType;

uint8_t initHiveStorage();
uint8_t initHiveSDStorage();
uint8_t initHiveEEPROMStorage();

extern void SDStorageOn();
extern void SDStorageOff();
    
template <class T> int writeStorage(int position, const T& value) {
  const byte *p = (const byte*)(const void*)&value;
  unsigned int i;
  
  if (StorageType == EEPROMStorage) {
    for (i = 0; i < sizeof(value); i++)
      EEPROM.write(position++, *p++);
    return i;
  }
  
  if (StorageType == SDStorage) {

    // DEBUG
    Serial.print(F("Writing to SD to file "));
    Serial.println(StorageFileName);

    SDStorageOn();
    File sdFile = SD.open(StorageFileName, FILE_WRITE);
    
    if (sdFile) {
      // DEBUG
      Serial.println(F("Opened file for writing"));

      sdFile.seek(position);
      for (i = 0; i < sizeof(value); i++)
        sdFile.write(*p++);
      sdFile.close();
      
      SDStorageOff();
      return i;
      
    } else {
      
      SDStorageOff();
      return -1;
      
    }
  }
  
  return -1;
}
    
template <class T> int readStorage(int position, T& value) {
  byte *p = (byte*)(void*)&value;
  unsigned int i;
    
  if (StorageType == EEPROMStorage) {
    for (i = 0; i < sizeof(value); i++)
      *p++ = EEPROM.read(position++);
    return i;
  }
    
  if (StorageType == SDStorage) {
    
    // DEBUG
    Serial.println(F("Reading from SD card"));

    SDStorageOn();
    File sdFile = SD.open(StorageFileName);
    
    if (sdFile) {
      
      // DEBUG
      Serial.println(F("File is open"));

      sdFile.seek(position);
      for (i = 0; i < sizeof(value); i++)
        *p++ = sdFile.read();
      sdFile.close();
      
      SDStorageOff();
      return i;
      
    }
  }
  
  return -1;   
}
    
#endif
