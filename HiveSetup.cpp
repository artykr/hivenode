#include "HiveSetup.h"
#include "LightSwitch.h"
#include "AppContext.h"

// Initialize global structure
SensorModule *sensorModuleArray[modulesCount];

void initModules(AppContext *context, boolean loadSettings) {
  int lastStoragePointer = settingsOffset;
  
  // Create objects for all sensor modules and init each one
  sensorModuleArray[0] = new LightSwitch(context, hallZone, moduleId, lastStoragePointer, loadSettings, 8, 4);
 
  lastStoragePointer += sensorModuleArray[0]->getStorageSize();
}