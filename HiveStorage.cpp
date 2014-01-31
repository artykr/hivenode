#include "HiveSetup.h"
#include "SD.h"
#include "EEPROM.h"
#include "HiveStorage.h"

uint8_t StorageType = EEPROMStorage;

// Returns
//  0 if storage isn't available
//  1 if storage is available and empty
//  2 if storage is available and filled
uint8_t initSDStorage() {
  uint8_t storageResult = 0;
  File sdFile;

  useDevice(DeviceIdSD);
  
  // DEBUG
  Serial.print("SD CS pin: ");
  Serial.println(DevicesCSPins[DeviceIdSD]);

  if (!SD.begin(DevicesCSPins[DeviceIdSD])) {
    Serial.println("Card init failed, or not present");
    // don't do anything more:
    return 0;
  }

  if (SD.exists(StorageFileName)) {
    // DEBUG
    Serial.println("Found settings file");

    File sdFile = SD.open(StorageFileName, FILE_READ);
    
    if (sdFile && sdFile.size() > 0) {
      // DEBUG
      Serial.println("File size > 0");
      sdFile.close();

      storageResult = 2;
    } else {
      // DEBUG
      Serial.println("File is empty");
      
      if (sdFile) {
        sdFile.close();
      }

      storageResult = 1;
    }
    
  } else {
    storageResult = 1;
  }
  
  return storageResult;
}

// Returns true if there are saved settings
// otherwise returns false
uint8_t initEEPROMStorage() {
  byte checkByte;
  checkByte = EEPROM.read(0);
  if (checkByte == StorageCheckByte) {
    return 1;
  } else {
    return 0;
  }
}

uint8_t initStorage() {
  uint8_t initResult = 0;
  
  // DEBUG
  Serial.print(F("Set up storage: "));

  // Check if there's an SD card available
  initResult = initSDStorage();
  
  if (initResult == 0) {
    // If no SD card then use EEPROM for storage
    StorageType = EEPROMStorage;
    
    // DEBUG
    Serial.println(F("EEPROM"));
    Serial.print(F("Init result: "));
    Serial.println(initResult);

    // Tell if we need to load settings
    return initEEPROMStorage();
  } else {
    // Use SD card if available
    StorageType = SDStorage;
    
    // We're going to store all system settings in EEPROM,
    // so there's no need for storage offset for SD card
    SettingsOffset = 0;
    
    // DEBUG
    Serial.println(F("SD"));
    Serial.print(F("Init result: "));
    Serial.println(initResult);

    // Tell if we need to load settings (0 or 1)
    return initResult - 1;
  }
}

uint8_t loadSystemSettings() {
  // Check if there are some settings stored in EEPROM
  if (initEEPROMStorage() > 0) {
    // Store current storage type
    // and override it temporarily to read settings from EEPROM
    uint8_t oldStorageType = StorageType;
    StorageType = EEPROMStorage;
    
    // Skip the first "check" byte and read settings
    // All settings are meant to be global variables
    for (int i = 1; i < 4; i++) {
      readStorage(i, hivemac[i+2]);
    }
    
    // Restore original storage type
    StorageType = oldStorageType; 
    return 1;
  } else {
    return 0;
  } 
}

void saveSystemSettings() {
  uint8_t oldStorageType = StorageType;
  StorageType = EEPROMStorage;

  // Write a "check" byte first
  // to indicate we have some settings stored in EEPROM
  writeStorage(0, StorageCheckByte);
  
  for (int i = 1; i < 4; i++) {
    writeStorage(i, hivemac[i+2]);
    // DEBUG
    Serial.println("Save system settings");
  }
  
  StorageType = oldStorageType;
}