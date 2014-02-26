/*
  DHTSwitch.h - Library for driving a device through a relay
  depending on DHT sensor values.
*/
  
#ifndef DHTSwitch_h
#define DHTSwitch_h
#define DHTSWITCH_MODULE_VERSION 1

#ifndef SWITCH_RELAY_ON
#define SWITCH_RELAY_ON 0
#define SWITCH_RELAY_OFF 1
#endif

// TODO: define STATE_ON 1 and STATE_OFF 0
// to use when reading and comparing light and switch state

#include "Arduino.h"
#include "SensorModule.h"
#include "aJson.h"
#include "AppContext.h"
#include "DHTSensor.h"
    
class DHTSwitch : public SensorModule
{
  public:
    DHTSwitch(
      AppContext *context,            // We need a context structure to get global properties, i.e. method for push notifications on a switch change etc.
      DHTSensor *sensor,              // Sensor object
      const byte zone,
      byte moduleId,
      int storagePointer,
      boolean loadSettings = true,    // We pass loadSettings flag = 0 in case if we are sure that there are no settings in the storage yet
      double tTreshold = 65535,       // Temperature treshold to switch the relay. 65535 value means no treshold.
      double hTreshold = 65535,       // Humidity treshold to switch the relay.
      unsigned int maxOnTime = 0,     // Time (in seconds) to keep the relay on. Zero value corresponds to no limit.
      unsigned int maxOffTime = 0,    // Time (in seconds) to keep the relay off. Only one of two values can be set at a time.
                                      // If both present then maxOn value takes precedence.
      int8_t switchType = 1,          // 1 - turn on the relay if temperature/humdity is higher than treshold, 0 - vice versa.
      int8_t relayPin = -1
    );
    
    const char* getModuleType();
    byte getStorageSize();
    void loopDo();
    void getJSONSettings();       // Fill-in module settings in JSON object through _context property
    boolean setJSONSettings(aJsonObject *moduleItem); // Update settings from JSON object through _context property

    void turnModuleOff();         // Turn module off
    void turnModuleOn();          // Turn module on
    
  private:
    // settings structure for use with write/readAnything routine
    typedef struct config_t
    {
      int8_t driveMode;           // Device switching mode: 0 - auto, 1 - manual on, 2 - manual off
      int8_t moduleState;         // int8 used to store invalid negative values for validation purposes
      double tTreshold;
      double hTreshold;
      int maxOnTime;
      int maxOffTime;
      int8_t switchType;
    };
    
    int8_t _driveMode;            // Device switching mode: 0 - auto, 1 - manual on, 2 - manual off
    int8_t _relayPin;             // Pin number for device control (relay)
    double _tTreshold;
    double _hTreshold;
    int _maxOnTime;
    int _maxOffTime;
    int8_t _switchType;           // 1 - switch on when crossing treshold from low to top
                                  // 0 - from top to low
    unsigned long _timeCounter;
    
    static const char _moduleType[12];   // Module type string

    boolean _stateChanged;        // Set to TRUE if anything (settings) - to prevent filling settings in again e.g. when the server asks for current settings
    boolean _previousDeviceState; // Save previous light state to switch only if changed
    AppContext *_context;         // AppContext object pointer
    DHTSensor *_sensor;           // Sensor object pointer

    void _saveSettings();         // Puts settings into storage
    void _loadSettings();         // Loads settings from storage
    void _resetSettings();        // Resets settings to default values
    byte _readDeviceState();      // Read the light (relay) state
    void _turnDeviceOff();        // Turn the light off (manual override)
    void _turnDeviceOn();         // Turn the light on (manual override)
    void _turnDeviceAuto();       // Turn the light automatically (reset the override)
    boolean _checkTreshold();     // Check if t/h values are lower/higher than treshold level (depending on switchType)
    void _pushNotify();           // Prepare data and call notification method from the main script
    
    boolean _validateSettings(config_t *settings);
};

#endif
