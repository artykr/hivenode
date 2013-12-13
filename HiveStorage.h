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
#include "DeviceDispatch.h"

extern uint8_t StorageType;

uint8_t initStorage();
uint8_t initSDStorage();
uint8_t initEEPROMStorage();
void saveSystemSettings();
uint8_t loadSystemSettings();
    
template <class T> int writeStorage(int position, const T& value) {
  const byte *p = (const byte*)(const void*)&value;
  unsigned int i;
  
  if (StorageType == EEPROMStorage) {
    Serial.print(F("Writing to EEPROM"));
    for (i = 0; i < sizeof(value); i++)
      EEPROM.write(position++, *p++);
    return i;
  }
  
  if (StorageType == SDStorage) {

    // DEBUG
    Serial.print(F("Writing to SD to file "));
    Serial.println(StorageFileName);
    
    // Select slave SPI device
    useDevice(DeviceIdSD);
    
    File sdFile = SD.open(StorageFileName, FILE_WRITE);
    
    if (sdFile) {
      // DEBUG
      Serial.println(F("Opened file for writing"));

      if (sdFile.seek(position)) {
        for (i = 0; i < sizeof(value); i++)
          sdFile.write(*p++);
      }
      
      sdFile.close();
      return i;
      
    } else {
      // DEBUG
      Serial.println("Unable to open storage file fo writing");

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

    useDevice(DeviceIdSD);
    
    File sdFile = SD.open(StorageFileName);
    
    if (sdFile) {
      
      // DEBUG
      Serial.println(F("File is open"));

      if (sdFile.seek(position)) {
        for (i = 0; i < sizeof(value); i++)
          *p++ = sdFile.read();
      }
      
      sdFile.close();
      return i;
    }
  }
  
  return -1;   
}
    
#endif
