/*
  PirSwitch.h - Library for driving a light with a wall-mounted switch
  through a relay.
*/
  
#ifndef PirSwitch_h
#define PirSwitch_h
#define PIRSWITCH_MODULE_VERSION 1

// TODO: change to const
#define SWITCH_RELAY_ON 0
#define SWITCH_RELAY_OFF 1

// TODO: define STATE_ON 1 and STATE_OFF 0
// to use when reading and comparing light and switch state

#include "Arduino.h"
#include "SensorModule.h"
#include "aJson.h"
#include "AppContext.h"
    
class PirSwitch : public SensorModule
{
  public:
    PirSwitch(AppContext *context, const byte zone, byte moduleId, int storagePointer, boolean loadSettings = true, int8_t switchPin = -1, int8_t lightPin = -1);
    // We need a context structure to get global properties, i.e. method for push notifications on a switch change etc.
    // We pass loadSettings flag = 0 in case if we are sure that there are no settings in EEPROM yet
    
    const char* getModuleType();
    byte getStorageSize();
    void loopDo();
    void getJSONSettings(); // Fill-in module settings in JSON object through _context property
    boolean setJSONSettings(aJsonObject *moduleItem); // Update settings from JSON object through _context property

    void turnModuleOff();        // Turn module off
    void turnModuleOn();         // Turn module on

  private:
    // settings structure for use with write/readStorage routine
    typedef struct config_t
    {
      int8_t lightMode;           // Light switching mode: 0 - auto, 1 - manual on, 2 - manual off
      int8_t pirDelay;            // Delay between PIR sensor state change and relay switch (in seconds)
      int8_t moduleState;         // Is module on or off
    };
    
    int8_t _lightMode;            // Light switching mode: 0 - auto, 1 - manual on, 2 - manual off
    int8_t _pirDelay;             // Delay between PIR sensor state change and relay switch (in seconds)
    int8_t _switchPin;            // Pin number for the PIR sensor
    int8_t _lightPin;             // Pin number for the relay
    unsigned long _delayCounter;
    
    static const char _moduleType[12];   // Module type string

    boolean _stateChanged;        // Set to TRUE if anything (settings) changes - to prevent filling settings in again e.g. when the server asks for current settings
    boolean _switchState;         // Current switch state. 1 = on, 0 = off relay-aware state
    boolean _previousLightState;  // Save previous light state to switch light only if changed
    boolean _previousSwitchState; // Helper for holding switch state
    AppContext *_context;         // Pointer to the AppContext object
    
    void _saveSettings();         // Puts settings into storage
    void _loadSettings();         // Loads settings from storage
    void _resetSettings();        // Resets settings to default values
    byte _readSwitchState();      // Read the switch state
    byte _readLightState();       // Read the light (relay) state
    void _turnLightOff();         // Turn the light off (manual override)
    void _turnLightOn();          // Turn the light on (manual override)
    void _turnLightAuto();        // Turn the light automatically (reset the override)
    void _pushNotify();           // Prepare data and call notification method from the main script
    
    boolean _validateSettings(config_t *settings);
};

#endif
