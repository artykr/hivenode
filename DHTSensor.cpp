#include "Arduino.h"
#include "HiveStorage.h"
#include "DHTSensor.h"
#include "SensorModule.h"
#include "aJson.h"
#include "AppContext.h"
#include "MemoryFree.h"
#include "DHT.h"
#include "HiveUtils.h"

const char DHTSensor::_moduleType[12] = "DHTSensor";

DHTSensor::DHTSensor(AppContext *context, const byte zone, byte moduleId, int storagePointer, boolean loadSettings, int8_t signalPin) :
  SensorModule(storagePointer, moduleId, zone),
  _signalPin(signalPin),
  _context(context) {

  _intervalCounter = millis();
  _stateChanged = true;
  _resetSettings();

  // DEBUG
  debugPrint(F("DHT: Init DHTSensor"));

  _dht.setup(signalPin);
  delay(_dht.getMinimumSamplingPeriod());

  if (loadSettings) {
    _loadSettings();
  } else {
    // If there's no load flag then we haven't saved anything yet, so init the storage
    _saveSettings();
  }

  if (_moduleState) {
    _temperature = _dht.getTemperature();

    if (isnan(_temperature) != 0) {
      _temperature = 65535;
    }

    _humidity = _dht.getHumidity();

    if (isnan(_humidity) != 0) {
      _humidity = 65535;
    }
  }

  // DEBUG
  debugPrint(F("DHT: Finished DHTSensor init"), true);
}

void DHTSensor::_resetSettings() {
  _moduleState = 1;
  _measureUnits = 0;
  _measureInterval = 30;
  _temperature = 65535;
  _humidity = 65535;
}

byte DHTSensor::getStorageSize() {
  return sizeof(config_t);
}

void DHTSensor::_loadSettings() {
  boolean isLoaded = false;

  //DEBUG
  debugPrint(F("DHT: Loading module settings"));

  config_t settings;

  // Try to read storage and validate
  if (readStorage(_storagePointer, settings) > 0) {
    if (_validateSettings(&settings)) {
      isLoaded = true;
    }
  }

  if (isLoaded) {
    _measureUnits = settings.measureUnits;
    _measureInterval = settings.measureInterval;
    _moduleState = settings.moduleState;
  } else {
    // If unable to read then reset settings to default
    // and try write them back to fix the storage
    // (suppose the settings file was corrupt in some way)
    _resetSettings();
    _saveSettings();
  }

  _stateChanged = true;
}

void DHTSensor::_saveSettings() {
  //DEBUG
  debugPrint(F("DHT: Saving module settings"));

  config_t settings;

  settings.measureUnits = _measureUnits;
  settings.measureInterval = _measureInterval;
  settings.moduleState = _moduleState;

  writeStorage(_storagePointer, settings);
}

double DHTSensor::getTemperature() {
  if (_measureUnits == 0) {
    return _temperature;
  } else {
    float f = (float) _temperature;
    double d = (double) _dht.toFahrenheit(f);
    return d;
  }
}

double DHTSensor::getHumidity() {
  return _humidity;
}

int8_t DHTSensor::getLowerBoundTemperature() {
  return _dht.getLowerBoundTemperature();
}

int8_t DHTSensor::getUpperBoundTemperature() {
  return _dht.getUpperBoundTemperature();
}

int8_t DHTSensor::getLowerBoundHumidity() {
  return _dht.getLowerBoundHumidity();
}

int8_t DHTSensor::getUpperBoundHumidity() {
  return _dht.getUpperBoundHumidity();
}

void DHTSensor::getJSONSettings() {
  // generate json settings only if something's changed
  // TODO: mark properties with read-only and read-write types

  if (_stateChanged) {

    aJsonObject *moduleItem = aJson.getArrayItem(*(_context->moduleCollection), moduleId-1);
    aJsonObject *moduleItemProperty = aJson.getObjectItem(moduleItem, "moduleType");

    // If we have an empty JSON settings structure
    // then fill it with values

    if (strcmp(moduleItemProperty->valuestring, _moduleType) != 0) {

      // TODO: Add prefixes to param names to mark readonly fields
      aJson.addStringToObject(moduleItem, "moduleType", _moduleType);
      aJson.addNumberToObject(moduleItem, "moduleState", _moduleState);
      aJson.addNumberToObject(moduleItem, "zoneId", _moduleZone);
      aJson.addNumberToObject(moduleItem, "temperature", _temperature);
      aJson.addNumberToObject(moduleItem, "humidity", _humidity);
      aJson.addNumberToObject(moduleItem, "measureUnits", _measureUnits);
      aJson.addNumberToObject(moduleItem, "measureInterval", _measureInterval);
      aJson.addNumberToObject(moduleItem, "tUpperBound", getUpperBoundTemperature());
      aJson.addNumberToObject(moduleItem, "tLowerBound", getLowerBoundTemperature());
      aJson.addNumberToObject(moduleItem, "hUpperBound", getUpperBoundHumidity());
      aJson.addNumberToObject(moduleItem, "hLowerBound", getLowerBoundHumidity());
    } else {
      // If we have an already initialized settings JSON structure
      // then just replace the values

      moduleItemProperty = aJson.getObjectItem(moduleItem, "moduleState");
      moduleItemProperty->valuebool = _moduleState;

      moduleItemProperty = aJson.getObjectItem(moduleItem, "temperature");

      moduleItemProperty->valuefloat = getTemperature();

      moduleItemProperty = aJson.getObjectItem(moduleItem, "humidity");
      moduleItemProperty->valuefloat = getHumidity();

      moduleItemProperty = aJson.getObjectItem(moduleItem, "measureUnits");
      moduleItemProperty->valueint = _measureUnits;

      moduleItemProperty = aJson.getObjectItem(moduleItem, "measureInterval");
      moduleItemProperty->valueint = _measureInterval;
    }

    _stateChanged = false;

  }
}

// TODO: if error, return settings object with error item
boolean DHTSensor::_validateSettings(config_t *settings) {
  if ((settings->measureUnits < 0) || (settings->measureUnits > 1)) {
    return false;
  }

  if ((settings->moduleState < 0) || (settings->moduleState > 1)) {
    return false;
  }

  if ((settings->measureInterval < _dht.getMinimumSamplingPeriod()) || (settings->measureInterval > 255)) {
    return false;
  }

  return true;
}

boolean DHTSensor::setJSONSettings(aJsonObject *moduleItem) {
  config_t settings;
  aJsonObject *moduleItemProperty;
  // Initialiaze to invalid values so the validation works
  int8_t newMeasureUnits = -1;
  uint8_t newMeasureInterval = 0;
  int8_t newModuleState = -1;

  // Check for module type first
  moduleItemProperty = aJson.getObjectItem(moduleItem, "moduleType");
  if (strcmp(moduleItemProperty->valuestring, _moduleType) != 0) {
    return false;
  }

  moduleItemProperty = aJson.getObjectItem(moduleItem, "moduleState");
  newModuleState = moduleItemProperty->valuebool;

  moduleItemProperty = aJson.getObjectItem(moduleItem,  "measureUnits");
  newMeasureUnits = moduleItemProperty->valueint;

  moduleItemProperty = aJson.getObjectItem(moduleItem,  "measureInterval");
  newMeasureInterval = moduleItemProperty->valueint;

  settings.measureUnits = newMeasureUnits;
  settings.measureInterval = newMeasureInterval;
  settings.moduleState = newModuleState;

  if (!_validateSettings(&settings)) {
    return false;
  }

  if (_moduleState != newModuleState) {
    newModuleState ? turnModuleOn() : turnModuleOff();
  }

  if ((_measureUnits != newMeasureUnits) || (_measureInterval != newMeasureInterval)) {
    _measureUnits = newMeasureUnits;
    _measureInterval = newMeasureInterval;
    _stateChanged = true;
    _saveSettings();
  }

  return true;
}

void DHTSensor::turnModuleOff() {
  if (_moduleState) {
    _temperature = 65535;
    _humidity = 65535;
    _moduleState = false;
    _stateChanged = true;
    _saveSettings();
  }
}

void DHTSensor::turnModuleOn() {
  if (!_moduleState) {

    _temperature = _dht.getTemperature();

    if (isnan(_temperature) != 0) {
      _temperature = 65535;
    }

    _humidity = _dht.getHumidity();

    if (isnan(_humidity) != 0) {
      _humidity = 65535;
    }

    _moduleState = true;
    _stateChanged = true;
    _saveSettings();
  }
}

void DHTSensor::loopDo() {
  // If the module is on now
  if (_moduleState) {
    // If it's time to measure
    if (timeDiff(_intervalCounter) >= (_measureInterval * 1000)) {

      // DEBUG
      debugPrint(F("DHT: Checking..."));

      // Reset time interval counter
      _intervalCounter = millis();

      // Get new values
      double newTemperature = _dht.getTemperature();
      double newHumidity = _dht.getHumidity();

      if (newTemperature != _temperature || newHumidity != _humidity) {
        if (isnan(newTemperature) == 0) {
          _temperature = newTemperature;
        }

        if (isnan(_humidity) == 0) {
          _humidity = newHumidity;
        }

        // DEBUG
        debugPrint(F("DHT: Temperature: "), false);
        debugPrint(_temperature);
        debugPrint(F("DHT: Humidity: "), false);
        debugPrint(_humidity);

        _stateChanged = true;
      }
    }
  }
}
