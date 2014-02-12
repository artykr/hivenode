#ifndef HiveSetup_h
#define HiveSetup_h

#include "SensorModule.h"
#include "AppContext.h"
#include "Ethernet.h"

// Uncomment to use static ip instead of DHCP
#ifndef HIVE_STATIC_IP
#define HIVE_STATIC_IP
#endif

extern IPAddress nodeIPAddress;

// Stored at the first byte of EEPROM indicates that settings are already
// written
const uint8_t StorageCheckByte = 99;
// Define buffer and buffer lentgth for incoming HTTP REST request processing
const uint8_t RestRequestLength = 255;

// TODO: use uint8_t everywhere instead of byte

// This PCB id
const byte nodeId = 1;

// Total number of modules on board
// All the modules should be described in initModules()
const byte modulesCount = 2;

// Define zones in accordance with physical locations
const byte hallZone = 1;
const byte kitchenZone = 2;
const byte zones[] = { hallZone, kitchenZone };

// ****
// INPORTANT: remember to connect SS pin from Ethernet module to pin 10
//            on Arduino Mega if using W5100 separate module (not a shield)
// ****

// Enumerate all SPI devices connected to board
// starting from 0
const uint8_t DeviceIdEthernet = 0;

// Change Ethernet CS pin in accordance with Ethernet module documentation
const uint8_t DeviceIdSD = 1;

// CS pins for each SPI device according to device ids
const uint8_t DevicesCSPins[] = { 10, 53 };

// Storage offset - modules settings come after the offset.
// This is the size of general settings (if any)
// One byte is always reserved for a setting presence flag
// General settings Re always stored in EEPROM.
// Default offset is used to store module settings in EEPROM.
// Offset could be changed if alternative storage (e.g. SD) is used
// for storing values.
extern uint8_t SettingsOffset;

const uint8_t EEPROMStorage = 1;
const uint8_t SDStorage = 2;

// No changeable parameters below this line

// Name string is assigned in cpp file
extern char StorageFileName[16];

// Define global structure to hold all sensor modules
extern SensorModule *sensorModuleArray[modulesCount];

// Define an array for MAC adress of the Ethernet shield
extern byte hivemac[6]; 

void initModules(AppContext *context, boolean loadSettings);

#endif