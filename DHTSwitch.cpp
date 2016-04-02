#include "Arduino.h"
#include "HiveStorage.h"
#include "DHTSwitch.h"
#include "SensorModule.h"
#include "aJson.h"
#include "AppContext.h"
#include "MemoryFree.h"
#include "HiveUtils.h"

const char DHTSwitch::_moduleType[12] = "DHTSwitch";

DHTSwitch::DHTSwitch(
  AppContext *context,
  DHTSensor *sensor,
  const byte zone,
  byte moduleId,
  int storagePointer,
  boolean loadSettings,
  double tThershold,
  double hThershold,
  int maxOnTime,
  int restTime,
  int8_t switchType,
  int8_t relayPin
  ) : SensorModule(storagePointer, moduleId, zone),
  _context(context),
  _sensor(sensor),
  _tThershold(tThershold),
  _hThershold(hThershold),
  _maxOnTime(maxOnTime),
  _restTime(restTime),
  _switchType(switchType),
  _relayPin(relayPin)
{

  _stateChanged = true;
  _resetSettings();

  // DEBUG
  debugPrint(F("DHTS: Init DHTSwitch"));

  if (loadSettings) {
    _loadSettings();
  } else {
    // If there's no load flag then we haven't saved anything yet, so init the storage
    _saveSettings();
  }

  pinMode(_relayPin, OUTPUT);

  if (!_moduleState) {
    digitalWrite(_relayPin, SWITCH_RELAY_OFF);
  } else {

    switch (_driveMode) {
      case 1:
        // If there's a manual "on" override
        digitalWrite(_relayPin, SWITCH_RELAY_ON);
        break;
      case 2:
        // If there's a manual "off" override
        digitalWrite(_relayPin, SWITCH_RELAY_OFF);
        break;
    }

    _previousDeviceState = _readDeviceState();
  }

  // DEBUG
  debugPrint(F("DHTS: Finished DHTswitch init"));
}

void DHTSwitch::_resetSettings() {
  _previousDeviceState = 0;
  _workStart = millis();
  _restStart = 0;
  _restMode = 0;
  _moduleState = 1;
  _driveMode = 0;
}

byte DHTSwitch::getStorageSize() {
  return sizeof(config_t);
}

void DHTSwitch::_loadSettings() {
  boolean isLoaded = false;

  //DEBUG
  debugPrint(F("DHTS: Loading module settings"));

  config_t settings;

  // Try to read storage and validate
  if (readStorage(_storagePointer, settings) > 0) {
    if (_validateSettings(&settings)) {
      isLoaded = true;
    }
  }

  if (isLoaded) {
    _driveMode = settings.driveMode;
    _moduleState = settings.moduleState;
    _tThershold = settings.tThershold;
    _hThershold = settings.hThershold;
    _maxOnTime = settings.maxOnTime;
    _restTime = settings.restTime;
    _switchType = settings.switchType;
  } else {
    // If unable to read then reset settings to default
    // and try write them back to fix the storage
    // (suppose the settings file was corrupt in some way)
    _resetSettings();
    _saveSettings();
  }

  _stateChanged = true;
}

void DHTSwitch::_saveSettings() {
  //DEBUG
  debugPrint(F("DHTS: Saving module settings"));

  config_t settings;

  settings.driveMode = _driveMode;
  settings.moduleState = _moduleState;
  settings.tThershold = _tThershold;
  settings.hThershold = _hThershold;
  settings.maxOnTime = _maxOnTime;
  settings.restTime = _restTime;
  settings.switchType = _switchType;
  writeStorage(_storagePointer, settings);
}

byte DHTSwitch::_readDeviceState () {

  // Relay on/off states may be inversed depending on relay type
  // So we take it into account and return simple values
  // 1 if the relay is on, 0 - if it's off

  if (digitalRead(_relayPin) == SWITCH_RELAY_ON) {
    return 1;
  } else {
    return 0;
  }
}

void DHTSwitch::_turnDeviceOff() {
  if (_driveMode != 2) {
    _driveMode = 2;

    // Refresh the light state information
    _previousDeviceState = _readDeviceState();
    _stateChanged = true;
    _saveSettings();
  }
}

void DHTSwitch::_turnDeviceOn() {
  if (_driveMode != 1) {
    _driveMode = 1;

    // Refresh the light state information
    _previousDeviceState = _readDeviceState();
    _stateChanged = true;
    _saveSettings();
  }
}

void DHTSwitch::_turnDeviceAuto() {
  _driveMode = 0;

  _stateChanged = true;
  _saveSettings();
}

void DHTSwitch::_pushNotify() {
  _context->pushNotify(moduleId);
}

void DHTSwitch::getJSONSettings() {
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
      aJson.addNumberToObject(moduleItem, "deviceState", _readDeviceState());
      aJson.addNumberToObject(moduleItem, "driveMode", _driveMode);
      aJson.addNumberToObject(moduleItem, "tThershold", _tThershold);
      aJson.addNumberToObject(moduleItem, "hThershold", _tThershold);
      aJson.addNumberToObject(moduleItem, "maxOnTime", _maxOnTime);
      aJson.addNumberToObject(moduleItem, "restTime", _restTime);
      aJson.addNumberToObject(moduleItem, "switchType", _switchType);
      aJson.addNumberToObject(moduleItem, "sensorId", _sensor->moduleId);

    } else {
      // If we have an already initialized settings JSON structure
      // then just replace the values

      moduleItemProperty = aJson.getObjectItem(moduleItem, "moduleState");
      moduleItemProperty->valuebool = _moduleState;

      moduleItemProperty = aJson.getObjectItem(moduleItem, "deviceState");
      moduleItemProperty->valueint = _readDeviceState();

      moduleItemProperty = aJson.getObjectItem(moduleItem, "driveMode");
      moduleItemProperty->valueint = _driveMode;

      moduleItemProperty = aJson.getObjectItem(moduleItem, "tThershold");
      moduleItemProperty->valuefloat = _tThershold;

      moduleItemProperty = aJson.getObjectItem(moduleItem, "hThershold");
      moduleItemProperty->valuefloat = _hThershold;

      moduleItemProperty = aJson.getObjectItem(moduleItem, "maxOnTime");
      moduleItemProperty->valueint = _maxOnTime;

      moduleItemProperty = aJson.getObjectItem(moduleItem, "restTime");
      moduleItemProperty->valueint = _restTime;

      moduleItemProperty = aJson.getObjectItem(moduleItem, "switchType");
      moduleItemProperty->valueint = _switchType;
    }

    _stateChanged = false;

  }
}

// TODO: if error, return settings object with error item
boolean DHTSwitch::_validateSettings(config_t *settings) {
  if ((settings->driveMode < 0) || (settings->driveMode > 2)) {
    return false;
  }

  if ((settings->moduleState < 0) || (settings->moduleState > 1)) {
    return false;
  }

  if (settings->tThershold < _sensor->getLowerBoundTemperature()) {
    return false;
  }

  if ((settings->tThershold > _sensor->getUpperBoundTemperature()) && (settings->tThershold != 65535)) {
    return false;
  }

  if (settings->hThershold < _sensor->getLowerBoundHumidity()) {
    return false;
  }

  if ((settings->hThershold > _sensor->getUpperBoundHumidity()) && (settings->hThershold != 65535)) {
    return false;
  }

  if ((settings->maxOnTime < 0) || (settings->restTime < 0)) {
    return false;
  }

  if ((settings->maxOnTime > 0) && (settings->restTime == 0)) {
    return false;
  }

  if ((settings->maxOnTime == 0) && (settings->restTime > 0)) {
    return false;
  }

  if ((settings->switchType < 0) || (settings->switchType > 1)) {
    return false;
  }

  return true;
}

boolean DHTSwitch::setJSONSettings(aJsonObject *moduleItem) {
  config_t settings;
  aJsonObject *moduleItemProperty;
  // Initialiaze to invalid values so the validation works
  int8_t newModuleState = -1;
  int8_t newDriveMode = -1;
  double newTThershold = -1;
  double newHThershold = -1;
  int newMaxOnTime = -1;
  int newRestTime = -1;
  int8_t newSwitchType = -1;

  // Check for module type first
  moduleItemProperty = aJson.getObjectItem(moduleItem, "moduleType");
  if (strcmp(moduleItemProperty->valuestring, _moduleType) != 0) {
    return false;
  }

  moduleItemProperty = aJson.getObjectItem(moduleItem, "moduleState");
  newModuleState = moduleItemProperty->valuebool;

  moduleItemProperty = aJson.getObjectItem(moduleItem,  "driveMode");
  newDriveMode = moduleItemProperty->valueint;

  moduleItemProperty = aJson.getObjectItem(moduleItem,  "tThershold");
  newTThershold = moduleItemProperty->valuefloat;

  moduleItemProperty = aJson.getObjectItem(moduleItem,  "hThershold");
  newHThershold = moduleItemProperty->valuefloat;

  moduleItemProperty = aJson.getObjectItem(moduleItem,  "maxOnTime");
  newMaxOnTime = moduleItemProperty->valueint;

  moduleItemProperty = aJson.getObjectItem(moduleItem,  "restTime");
  newRestTime = moduleItemProperty->valueint;

  moduleItemProperty = aJson.getObjectItem(moduleItem,  "switchType");
  newSwitchType = moduleItemProperty->valueint;

  settings.driveMode = newDriveMode;
  settings.moduleState = newModuleState;
  settings.tThershold = newTThershold;
  settings.hThershold = newHThershold;
  settings.maxOnTime = newMaxOnTime;
  settings.restTime = newRestTime;
  settings.switchType = newSwitchType;


  if (!_validateSettings(&settings)) {
    return false;
  }

  _tThershold = newTThershold;
  _hThershold = newHThershold;
  _maxOnTime = newMaxOnTime;
  _restTime = newRestTime;
  _switchType = newSwitchType;

  if (_moduleState != newModuleState) {
    newModuleState ? turnModuleOn() : turnModuleOff();
  }

  if (_driveMode != newDriveMode) {
    switch (newDriveMode) {
      case 0:
        _turnDeviceAuto();
        break;
      case 1:
        _turnDeviceOn();
        break;
      case 2:
        _turnDeviceOff();
        break;
    }
  }

  return true;
}

void DHTSwitch::turnModuleOff() {
  if (_moduleState) {
    digitalWrite(_relayPin, SWITCH_RELAY_OFF);
    _moduleState = false;
    _stateChanged = true;
    _saveSettings();
  }
}

void DHTSwitch::turnModuleOn() {
  if (!_moduleState) {

    // Check current operation mode (auto or manual override)
    if (_driveMode > 0) {
      _driveMode == 1 ? digitalWrite(_relayPin, SWITCH_RELAY_ON) : digitalWrite(_relayPin, SWITCH_RELAY_OFF);
    }

    _moduleState = true;
    _stateChanged = true;
    _saveSettings();
  }
}

boolean DHTSwitch::_checkThershold() {
  // It there's a thershold for temperature
  if (_tThershold != 65535) {
    if ((_sensor->getTemperature() > _tThershold) && (_switchType == 1)) {
      return true;
    }

    if ((_sensor->getTemperature() < _tThershold) && (_switchType == 0)) {
      return true;
    }
  }

  if (_hThershold != 65535) {
    if ((_sensor->getHumidity() > _hThershold) && (_switchType == 1)) {
      return true;
    }

    if ((_sensor->getHumidity() < _hThershold) && (_switchType == 0)) {
      return true;
    }
  }

  return false;
}

void DHTSwitch::loopDo() {
  int8_t deviceState;

  // If the module is on now
  if (_moduleState) {
    deviceState = _readDeviceState();

    // If auto-control is on
    if (_driveMode == 0) {

      // Check if device is having a rest
      if (_restMode == 1) {

        // If the rest is over
        if (timeDiff(_restStart) > _restTime * 1000) {
          _restMode = 0;
        } else {
          return;
        }

      } else {
        // If we have a time limit and it's time to rest
        // Also make sure device is working
        if ((deviceState == 1) && (_maxOnTime > 0) && timeDiff(_workStart) > _maxOnTime * 1000) {
          _restMode = 1;
          _restStart = millis();
          digitalWrite(_relayPin, SWITCH_RELAY_OFF);
          return;
        }
      }

      if (_checkThershold()) {
        if (deviceState == 0) {
          _workStart = millis();
          digitalWrite(_relayPin, SWITCH_RELAY_ON);
        }
      } else {
        digitalWrite(_relayPin, SWITCH_RELAY_OFF);
      }

    } else if ((_driveMode == 1) && (deviceState == 0)) {
      digitalWrite(_relayPin, SWITCH_RELAY_ON);
    } else if ((_driveMode == 2) && (deviceState == 1)) {
      digitalWrite(_relayPin, SWITCH_RELAY_OFF);
    }
  }

}
