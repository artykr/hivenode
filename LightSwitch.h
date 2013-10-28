/*
  LightSwitch.h - Library for driving a light with a wall-mounted switch
  through a relay.
*/
  
#ifndef LightSwitch_h
#define LightSwitch_h
#define LIGHTSWITCH_MODULE_VERSION 1

// TODO: change to const
#define LIGHTSWITCH_RELAY_ON 0
#define LIGHTSWITCH_RELAY_OFF 1

// TODO: define STATE_ON 1 and STATE_OFF 0
// to use when reading and comparing light and switch state

#include "Arduino.h"
#include "SensorModule.h"
#include "aJson.h"
#include "AppContext.h"
    
class LightSwitch : public SensorModule
{
  public:
    LightSwitch(AppContext *context, const byte zone, byte moduleId, int storagePointer, boolean loadSettings = true, int8_t switchPin = -1, int8_t lightPin = -1);
    // We need a context structure to get global properties, i.e. method for push notifications on a switch change etc.
    // We pass loadSettings flag = 0 in case if we are sure that there are no settings in EEPROM yet
    
    const char* getModuleType();
    byte getStorageSize();
    void loopDo();
    void getJSONSettings(); // Fill-in module settings in JSON object through _context property
    boolean setJSONSettings(aJsonObject *moduleItem); // Update settings from JSON object through _context property

    void turnModuleOff();        // Turn module off
    void turnModuleOn();         // Turn module on
    
    // DEBUG
    int DEBUG_memoryFree();

  private:
    // settings structure for use with write/readAnything routine
    typedef struct config_t
    {
      int8_t lightMode;           // Light switching mode: 0 - auto, 1 - manual on, 2 - manual off
      int8_t moduleState;         // int8 used to store invalid values for validation purposes
    };
    
    int8_t _lightMode;            // Light switching mode: 0 - auto, 1 - manual on, 2 - manual off
    int8_t _switchPin;            // Pin number for the switch
    int8_t _lightPin;             // Pin number for the light control (relay)
    
    static const char _moduleType[12];   // Module type string

    boolean _stateChanged;        // Set to TRUE if anything (settings) - to prevent filling settings in again e.g. when the server asks for current settings
    byte _debounceTime;           // Debounce time (ms)
    long _debounceCounter; 
    boolean _previousSwitchState; // Save previous state for debounce to work
    boolean _switchState;         // Current switch state. 1 = on, 0 = off relay-aware state
    boolean _previousLightState;  // Save previous light state to switch only if changed
    AppContext *_context;         // Pointer to the AppContext object
    
    void _saveSettings();         // Puts settings into EEPROM
    void _loadSettings();         // Loads settings from EEPROM
    byte _readSwitchState();      // Read the switch state
    byte _readLightState();       // Read the light (relay) state
    void _turnLightOff();         // Turn the light off (manual override)
    void _turnLightOn();          // Turn the light on (manual override)
    void _turnLightAuto();        // Turn the light automatically (reset the override)
    void _pushNotify();           // Prepare data and call notification method from the main script
    
    boolean _validateSettings(config_t *settings);
};

#endif
