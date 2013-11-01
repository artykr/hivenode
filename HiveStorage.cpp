#include "HiveSetup.h"
#include "SD.h"
#include "EEPROM.h"

uint8_t StorageType = EEPROMStorage;

void SDStorageOn() {
  // Disable Ethernet on SPI first
  // so we can talk to SD
  digitalWrite(EthernetCSPin, HIGH);
  // and enable SD
  digitalWrite(SDCSPin, LOW);
}

void SDStorageOff() {
  // Disable sD on SPI first
  // so we can talk to Ethernet
  digitalWrite(SDCSPin, HIGH);
  // and enable Ethernet
  digitalWrite(EthernetCSPin, LOW);
}

// Returns
//  0 if storage isn't available
//  1 if storage is available and empty
//  2 if storage is available and filled
uint8_t initHiveSDStorage() {
  uint8_t storageResult = 0;
  Sd2Card card;
  SdVolume volume;
  char buffer[16];

  // Set both CS pins to output just in case
  pinMode(SDCSPin, OUTPUT);
  pinMode(EthernetCSPin, OUTPUT);
  
  SDStorageOn();
  
  if (!card.init(SPI_HALF_SPEED, SDCSPin)) {
    storageResult = 0;
  }
  
  if (!volume.init(card)) {
    storageResult = 0;
  }
  
  strncpy(buffer, StorageFileName, sizeof(buffer));

  if (SD.exists(buffer)) {
    storageResult = 2;
  } else {
    storageResult = 1;
  }
  
  SDStorageOff();
  
  return storageResult;
}

// Returns true if there are saved settings
// otherwise returns false
uint8_t initHiveEEPROMStorage() {
  byte checkByte;
  checkByte = EEPROM.read(0);
  if (checkByte == StorageCheckByte) {
    return 1;
  } else {
    return 0;
  }
}

uint8_t initHiveStorage() {
  uint8_t initResult = 0;
  
  // Check if there's an SD card available
  initResult = initHiveSDStorage();
  
  if (initResult == 0) {
    // If no SD card then use EEPROM for storage
    StorageType = EEPROMStorage;
    // Tell if we need to load settings
    return initHiveEEPROMStorage();
  } else {
    // Use SD card if available
    StorageType = SDStorage;
    // Tell if we need to load settings (0 or 1)
    return initResult - 1;
  }
}