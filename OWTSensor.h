/*
  OWTSensor.h - Library for getting temperature and humidity from 1WT series sensor.
*/

#ifndef OWTSensor_h
#define OWTSensor_h
#define OWTSENSOR_MODULE_VERSION 1

// TODO: define STATE_ON 1 and STATE_OFF 0
// to use when reading and comparing light and switch state

#include "Arduino.h"
#include "SensorModule.h"
#include "aJson.h"
#include "AppContext.h"

#include "OneWire.h"
#include "DallasTemperature.h"

class OWTSensor : public SensorModule
{
  public:
    // Constructor for a known sensor address
    OWTSensor(AppContext *context, const byte zone, byte moduleId, int storagePointer, boolean loadSettings = true, int8_t signalPin = -1, int8_t resolution = 9, uint8_t deviceIndex = 0);

    // We need a context structure to get global properties, i.e. method for push notifications on a switch change etc.
    // We pass loadSettings flag = 0 in case if we are sure that there are no settings in storage yet
    // Resolution is the number of bits for the temperature value

    const char* getModuleType();
    byte getStorageSize();
    void loopDo();
    void getJSONSettings(); // Fill-in module settings in JSON object through _context property
    boolean setJSONSettings(aJsonObject *moduleItem); // Update settings from JSON object through _context property

    void turnModuleOff();         // Turn module off
    void turnModuleOn();          // Turn module on
    double getTemperature();      // Returns last measured temperature value

  private:
    // settings structure for use with write/readAnything routine
    typedef struct config_t
    {
      int8_t measureUnits;        // Measurment units: 0 - Celcius, 1 - Fahrenheit
      int8_t moduleState;         // int8 used to store invalid values for validation purposes
    };

    uint8_t _deviceIndex;         // 1-Wire device index
    int8_t _measureUnits;         // Measurment units: 0 - Celcius, 1 - Fahrenheit
    int8_t _signalPin;            // Signal pin number
    uint8_t _measureInterval;     // Measuring interval (seconds)
    double _temperature;          // Stores last measured temperature value. Value of 65535 means no last value is known
    int8_t _resolution;
    unsigned long _intervalCounter;        //

    static const char _moduleType[12];   // Module type string

    boolean _stateChanged;        // Set to TRUE if anything (settings) - to prevent filling settings in again
                                  // e.g. when the server asks for current settings
    AppContext *_context;         // Pointer to the AppContext object
    DallasTemperature *_dt;       // Dallas library instance
    OneWire *_oneWire;
    void _saveSettings();         // Puts settings into storage
    void _loadSettings();         // Loads settings from storage
    void _resetSettings();        // Resets settings to default values

    boolean _validateSettings(config_t *settings);
};

#endif
