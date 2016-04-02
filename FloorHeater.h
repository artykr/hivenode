/*
  Heater.h - Library for driving a heater through a relay.
*/

#ifndef FloorHeater_h
#define FloorHeater_h
#define FLOORHEATER_MODULE_VERSION 1

// Heater is driven by a hi-load SSR with direct control
#define FLOORHEATER_RELAY_ON 1
#define FLOORHEATER_RELAY_OFF 0

#include "Arduino.h"
#include "SensorModule.h"
#include "aJson.h"
#include "AppContext.h"
#include "OWTSensor.h"
#include "PID.h"
#include "ds3231.h"

class FloorHeater : public SensorModule
{
  public:
    FloorHeater(
      AppContext *context,
      OWTSensor *sensor,
      const byte zone,
      byte moduleId,
      int storagePointer,
      uint8_t devicePin,
      uint8_t timer,
      boolean loadSettings = true,
      float tMin = 23,
      float tMax = 30
    );

    const char* getModuleType();
    byte getStorageSize();
    void loopDo();
    void getJSONSettings();       // Fill-in module settings in JSON object through _context property
    boolean setJSONSettings(aJsonObject *moduleItem); // Update settings from JSON object through _context property

    void turnModuleOff();         // Turn module off
    void turnModuleOn();          // Turn module on
    void handleInterrupt();

  private:
    // settings structure for use with write/readAnything routine
    typedef struct config_t
    {
      int8_t moduleState;         // int8 used to store invalid negative values for validation purposes
      int8_t driveMode;           // 0 - schedule mode, 1 - manual temperature mode, 2 - off
      float setpoint;
      int schedule[7][3][3]; // Heating schedule 7 dow x 3 intervals x 3 values
      float kP;
      float kI;
      unsigned long stableTime;   // Time to stabilize at the setpoint
      unsigned long lastTuning;   // Timestamp of the last taining time
    };

    uint8_t _devicePin;
    uint8_t _timer;
    float _tMax;                  // Maximum heater temperature
    float _tMin;
    float _input;
    float _output;
    volatile float _outputTime;   // A temporary storage for _output since we cannot mark _output itself as volatile
    float _setpoint;
    float _kP;
    float _kI;
    unsigned long _stableTime;
    int _schedule[7][3][3];
    int8_t _deviceState;          // 0 - idle, 1 - working, 2 - tuning
    volatile int8_t _driveMode;
    unsigned long _lastTuning;
    unsigned long _windowStartTime;
    unsigned long _lastControlTime;
    boolean _stateChanged;        // Set to TRUE if anything (settings) - to prevent filling settings in again e.g. when the server asks for current settings
    struct ts _time;              // Time structure for the DS3231 library
    OWTSensor *_sensor;
    PID *_controller;
    AppContext *_context;         // Pointer to the AppContext object

    static const char _moduleType[15];   // Module type string
    static const uint16_t _OutputWindowSize = 10000;
    static const uint8_t _DriveTime = 50;
    static const uint16_t _ControlTime = 1000;

    void _saveSettings();         // Puts settings into storage
    void _loadSettings();         // Loads settings from storage
    void _resetSettings();        // Resets settings to default values
    void _resetTuning();
    byte _readDeviceState();      // Read the relay state
    void _turnDeviceOff();        // Turn the heater off (manual off override)
    void _turnDeviceOn();         // Turn the heater on (manual temperature override)
    void _turnDeviceBySchedule(); // Heat in schedule mode (reset the override)
    void _turnDeviceTuning();
    void _initTimer3();
    void _initTimer4();
    void _initTimer5();
    void _pushNotify();           // Prepare data and call notification method from the main script

    boolean _validateSettings(config_t *settings);
    boolean _checkSchedule();
};

#endif
