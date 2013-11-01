#ifndef HiveSettings_h
#define HiveSettings_h

#include "SensorModule.h"
#include "AppContext.h"

// Stored at the first byte of eeprom indicates that settings are already
// written
const uint8_t StorageCheckByte = 99;
// Define buffer and buffer lentgth for incoming HTTP REST request processing
const uint8_t RestRequestLength = 256;

// This PCB id
const byte nodeId = 1;

// Total number of modules on board
// All the modules should be described in initModules()
const byte modulesCount = 1;

// Define zones in accordance with physical locations
const byte hallZone = 1;
const byte kitchenZone = 2;
const byte zones[] = { hallZone, kitchenZone };

// ****
// INPORTANT: remember to connect SS pin from Ethernet module to pin 10
//            on Arduino Mega if using W5100 separate module (not a shield)
// ****

// SD card cable select pin
const uint8_t SDCSPin = 53;

// Change Ethernet CS pin in accordance with Ethernet module documentation
const uint8_t EthernetCSPin = 10;

// Storage offset - modules settings come after the offset.
// This is the size of general settings (if any)
// One byte is always reserved for a setting presence flag
const byte SettingsOffset = 4;

const uint8_t EEPROMStorage = 1;
const uint8_t SDStorage = 2;

// No changeable parameters below this line

// Name string is assigned in cpp file
extern char StorageFileName[16];

// Define global structure to hold all sensor modules
extern SensorModule *sensorModuleArray[modulesCount];

void initModules(AppContext *context, boolean loadSettings);

#endif