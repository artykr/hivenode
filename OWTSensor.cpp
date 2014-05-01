#include "Arduino.h"
#include "HiveStorage.h"
#include "OWTSensor.h"
#include "SensorModule.h"
#include "aJson.h"
#include "AppContext.h"
#include "MemoryFree.h"
#include "OneWire.h"
#include "DallasTemperature.h"
#include "HiveUtils.h"

const char OWTSensor::_moduleType[12] = "OWTSensor";

OWTSensor::OWTSensor(AppContext *context, const byte zone, byte moduleId, int storagePointer, boolean loadSettings, int8_t signalPin, int8_t resolution, uint8_t deviceIndex) :
  SensorModule(storagePointer, moduleId, zone),
  _signalPin(signalPin),
  _resolution(resolution),
  _context(context),
  _deviceIndex(deviceIndex) {

  OneWire oneWire(signalPin);
  DallasTemperature dt(&oneWire);

  _intervalCounter = millis();
  _stateChanged = true;
  _resetSettings();

   if (loadSettings) {
    _loadSettings();
  } else {
    // If there's no load flag then we haven't saved anything yet, so init the storage
    _saveSettings();
  }

  // DEBUG
  Serial.println(F("Init OWTSensor"));
  dt.begin();

  uint8_t deviceCount = dt.getDeviceCount();
  DeviceAddress deviceAddress;

  if (deviceCount == 0) {
    _moduleState = 0;
    _temperature = 65535;
  }

  if (deviceCount > 0) {
    if (!dt.getAddress(deviceAddress, 0)) {
      _moduleState = 0;
      _temperature = 65535;
    }
  }

  if (_moduleState) {
    dt.setWaitForConversion(true);
    dt.setResolution(deviceAddress, 9);
    _temperature = dt.getTempC(deviceAddress);

    if (isnan(_temperature) != 0) {
      _temperature = 65535;
    }

    dt.setWaitForConversion(false);
    _measureInterval = 750 / (1 << (12 - _resolution));

    _dt = &dt;
  }

  // DEBUG
  Serial.println(F("Finished OWTSensor init"));
}

void OWTSensor::_resetSettings() {
  _moduleState = 1;
  _measureUnits = 0;
  _temperature = 65535;
}

byte OWTSensor::getStorageSize() {
  return sizeof(config_t);
}

void OWTSensor::_loadSettings() {
  boolean isLoaded = false;

  //DEBUG
  Serial.println(F("Loading module settings"));

  config_t settings;

  // Try to read storage and validate
  if (readStorage(_storagePointer, settings) > 0) {
    if (_validateSettings(&settings)) {
      isLoaded = true;
    }
  }

  if (isLoaded) {
    _measureUnits = settings.measureUnits;
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

void OWTSensor::_saveSettings() {
  //DEBUG
  Serial.println(F("Saving module settings"));

  config_t settings;

  settings.measureUnits = _measureUnits;
  settings.moduleState = _moduleState;

  writeStorage(_storagePointer, settings);
}

double OWTSensor::getTemperature() {
  if (_measureUnits == 0) {
    return _temperature;
  } else {
    float f = (float) _temperature;
    double d = (double) DallasTemperature::toFahrenheit(f);
    return d;
  }
}

void OWTSensor::getJSONSettings() {
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
      aJson.addNumberToObject(moduleItem, "measureUnits", _measureUnits);
    } else {
      // If we have an already initialized settings JSON structure
      // then just replace the values

      moduleItemProperty = aJson.getObjectItem(moduleItem, "moduleState");
      moduleItemProperty->valuebool = _moduleState;

      moduleItemProperty = aJson.getObjectItem(moduleItem, "temperature");
      moduleItemProperty->valuefloat = getTemperature();

      moduleItemProperty = aJson.getObjectItem(moduleItem, "measureUnits");
      moduleItemProperty->valueint = _measureUnits;
    }

    _stateChanged = false;

  }
}

// TODO: if error, return settings object with error item
boolean OWTSensor::_validateSettings(config_t *settings) {
  if ((settings->measureUnits < 0) || (settings->measureUnits > 1)) {
    return false;
  }

  if ((settings->moduleState < 0) || (settings->moduleState > 1)) {
    return false;
  }

  return true;
}

boolean OWTSensor::setJSONSettings(aJsonObject *moduleItem) {
  config_t settings;
  aJsonObject *moduleItemProperty;
  // Initialiaze to invalid values so the validation works
  int8_t newMeasureUnits = -1;
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

  settings.measureUnits = newMeasureUnits;
  settings.moduleState = newModuleState;

  if (!_validateSettings(&settings)) {
    return false;
  }

  if (_moduleState != newModuleState) {
    newModuleState ? turnModuleOn() : turnModuleOff();
  }

  if (_measureUnits != newMeasureUnits) {
    _measureUnits = newMeasureUnits;
    _stateChanged = true;
    _saveSettings();
  }

  return true;
}

void OWTSensor::turnModuleOff() {
  if (_moduleState) {
    _temperature = 65535;
    _moduleState = false;
    _stateChanged = true;
    _saveSettings();
  }
}

void OWTSensor::turnModuleOn() {
  if (!_moduleState) {

    _temperature = _dt->getTempCByIndex(_deviceIndex);

    if (isnan(_temperature) != 0) {
      _temperature = 65535;
    }

    _moduleState = true;
    _stateChanged = true;
    _saveSettings();
  }
}

void OWTSensor::loopDo() {
  // If the module is on now
  if (_moduleState) {
    // If it's time to measure
    if (timeDiff(_intervalCounter) >= _measureInterval) {

      // Get new values
      double newTemperature = _dt->getTempCByIndex(_deviceIndex);

      if (newTemperature != _temperature) {
        if (isnan(newTemperature) == 0) {
          _temperature = newTemperature;
        }

        _stateChanged = true;
      }

      // Reset time interval counter
      _intervalCounter = millis();
      _dt->requestTemperaturesByIndex(_deviceIndex); 
    }
  }
}
