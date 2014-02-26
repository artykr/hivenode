#include "SensorModule.h"
#include "Arduino.h"

SensorModule::SensorModule(int storagePointer, byte moduleId, byte moduleZone) :
  _storagePointer(storagePointer),
  _moduleState(false),
  moduleId(moduleId),
  _moduleZone(moduleZone)
{}
