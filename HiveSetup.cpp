#include "HiveSetup.h"
#include "LightSwitch.h"

// Initialize global structures
SensorModule *sensorModuleArray[modulesCount];

// Settings storage file name (for SD card storage)
char StorageFileName[16] = "settings.txt"; // 15 characters long maximum

void initModules(AppContext *context, boolean loadSettings) {
  int lastStoragePointer = SettingsOffset;
  
  // Create objects for all sensor modules and init each one
  sensorModuleArray[0] = new LightSwitch(context, hallZone, 1, lastStoragePointer, loadSettings, 8, 4);
 
  lastStoragePointer += sensorModuleArray[0]->getStorageSize();
  
  // DEBUG
  Serial.println(F("Set up modules array"));
}