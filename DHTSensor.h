/*
  DHTSensor.h - Library for getting temperature and humidity from DHT series sensor.
*/
  
#ifndef DHTSensor_h
#define DHTSensor_h
#define DHTSENSOR_MODULE_VERSION 1

// TODO: define STATE_ON 1 and STATE_OFF 0
// to use when reading and comparing light and switch state

#include "Arduino.h"
#include "SensorModule.h"
#include "aJson.h"
#include "AppContext.h"

#define OPTIMIZE_SRAM_SIZE 1
#include "DHT.h"
    
class DHTSensor : public SensorModule
{
  public:
    DHTSensor(AppContext *context, const byte zone, byte moduleId, int storagePointer, boolean loadSettings = true, int8_t signalPin = -1);
    // We need a context structure to get global properties, i.e. method for push notifications on a switch change etc.
    // We pass loadSettings flag = 0 in case if we are sure that there are no settings in storage yet
    
    const char* getModuleType();
    byte getStorageSize();
    void loopDo();
    void getJSONSettings(); // Fill-in module settings in JSON object through _context property
    boolean setJSONSettings(aJsonObject *moduleItem); // Update settings from JSON object through _context property

    void turnModuleOff();         // Turn module off
    void turnModuleOn();          // Turn module on
    double getTemperature();      // Returns last measured temperature value
    double getHumidity();         // Returns last measured humidity value  
    int8_t getLowerBoundTemperature();
    int8_t getUpperBoundTemperature();
    int8_t getLowerBoundHumidity();
    int8_t getUpperBoundHumidity();  

  private:
    // settings structure for use with write/readAnything routine
    typedef struct config_t
    {
      int8_t measureUnits;        // Measurment units: 0 - Celcius, 1 - Fahrenheit
      int8_t moduleState;         // int8 used to store invalid values for validation purposes
      uint8_t measureInterval;    // Measuring interval (seconds, from 1 to 255)
    };
    
    int8_t _measureUnits;         // Measurment units: 0 - Celcius, 1 - Fahrenheit
    int8_t _signalPin;            // Signal pin number
    uint8_t _measureInterval;     // Measuring interval (seconds)
    double _temperature;          // Stores last measured temperature value. Value of 65535 means no last value is known
    double _humidity;             // Stores last measured humidity value. Value of 65535 means no last value is known
    long _intervalCounter;        // 
    
    static const char _moduleType[12];   // Module type string

    boolean _stateChanged;        // Set to TRUE if anything (settings) - to prevent filling settings in again
                                  // e.g. when the server asks for current settings
    AppContext *_context;         // Pointer to the AppContext object

    DHT _dht;                     // DHT library instance
    
    void _saveSettings();         // Puts settings into storage
    void _loadSettings();         // Loads settings from storage
    void _resetSettings();        // Resets settings to default values
    void _pushNotify();           // Prepare data and call notification method from the main script
    
    boolean _validateSettings(config_t *settings);
};

#endif
