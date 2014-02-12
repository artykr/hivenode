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
  int8_t i = 0;
  File myFile;
  
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

    myFile = SD.open(StorageFileName, FILE_WRITE);
    
    if (myFile) {
      // DEBUG
      Serial.println(F("Opened file for writing"));

      if (myFile.seek(position)) {
        for (i = 0; i < sizeof(value); i++)
          myFile.write(*p++);
      } else {
        i = -1;
      }
      
      myFile.close();
      return i;
      
    } else {
      // DEBUG
      Serial.println("Unable to open storage file fo writing");
      
      myFile.close();

      return -1;
    }
  }
  
  return -1;
}
    
template <class T> int readStorage(int position, T& value) {
  byte *p = (byte*)(void*)&value;
  int8_t i = 0;
  File myFile;

  if (StorageType == EEPROMStorage) {
    for (i = 0; i < sizeof(value); i++)
      *p++ = EEPROM.read(position++);
    return i;
  }
    
  if (StorageType == SDStorage) {
    
    // DEBUG
    Serial.println(F("Reading from SD card"));

    useDevice(DeviceIdSD);
    
    myFile = SD.open(StorageFileName);
    
    if (myFile) {
      
      // DEBUG
      Serial.println(F("File is open"));

      if ((position + sizeof(value)) <= myFile.size()) {

        if (myFile.seek(position)) {
          for (i = 0; i < sizeof(value); i++)
            *p++ = myFile.read();
        }

      } else {
        i = -1;

        // DEBUG
        Serial.println(F("ERROR: Value position is beyond the file size"));
      }
      
      myFile.close();
      return i;
    }

    myFile.close();
  }
  
  return -1;   
}
    
#endif
