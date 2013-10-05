/*
   Base class for sensor and actuator modules
*/

#ifndef SensorModule_h

#define SensorModule_h
#define SENSORMODULE_MODULE_VERSION 1

#include "Arduino.h"
#include "aJson.h"

class SensorModule {
  public:
    SensorModule(int storagePointer, byte moduleId, byte moduleZone);
    
    // We should define a default body for each virtual funcion
    // or declare them as pure virtual so the linker won't nag
    // with vtable errors

    virtual byte getStorageSize() { return 0; };    // Get constant value of storage size
    virtual void getJSONSettings() {};              // Fill-in module settings in JSON object
    virtual boolean setJSONSettings(aJsonObject *moduleItem) { return false; }; // Update settings from JSON object
    virtual void turnModuleOff() {};                // Turn module off
    virtual void turnModuleOn()  {};                // Turn module on
    virtual void loopDo() {};                       // Main processing (called from loop() in main sketch)

  protected:
    int _storagePointer;    // Storage address in EEPROM, set on object creation
    boolean _moduleState;   // Tells if the whole module is enabled (1) or disabled (0)
    byte _moduleId;         // Unique module ID, set on object creation
    byte _moduleZone;       // Zone code, where the module is located (typically a room), set on object creation
};

#endif