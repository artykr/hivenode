#include "Arduino.h"
#include "HiveStorage.h"
#include "DHTSwitch.h"
#include "SensorModule.h"
#include "aJson.h"
#include "AppContext.h"
#include "MemoryFree.h"

const char DHTSwitch::_moduleType[12] = "DHTSwitch";

DHTSwitch::DHTSwitch(
  AppContext *context,
  DHTSensor *sensor,
  const byte zone,
  byte moduleId,
  int storagePointer,
  boolean loadSettings,
  double tTreshold,
  double hTreshold,
  unsigned int maxOnTime,
  unsigned int maxOffTime,
  int8_t switchType,
  int8_t relayPin
  ) : SensorModule(storagePointer, moduleId, zone),
  _context(context),
  _sensor(sensor),
  _tTreshold(tTreshold),
  _hTreshold(hTreshold),
  _maxOnTime(maxOnTime),
  _maxOffTime(maxOffTime),
  _switchType(switchType),
  _relayPin(relayPin)
{
  
  _stateChanged = true;
  _resetSettings();
  
  // DEBUG
  Serial.println(F("Init DHTSwitch"));
  
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
  Serial.println(F("Finished DHTswitch init"));
}

void DHTSwitch::_resetSettings() {
  _previousDeviceState = 0;
  _moduleState = 1;
  _driveMode = 0;
}

byte DHTSwitch::getStorageSize() {
  return sizeof(config_t);
}

void DHTSwitch::_loadSettings() {
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
    _driveMode = settings.driveMode;
    _moduleState = settings.moduleState;
    _tTreshold = settings.tTreshold;
    _hTreshold = settings.hTreshold;
    _maxOnTime = settings.maxOnTime;
    _maxOffTime = settings.maxOffTime;
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
  Serial.println(F("Saving module settings"));

  config_t settings;

  settings.driveMode = _driveMode;
  settings.moduleState = _moduleState;
  settings.tTreshold = _tTreshold;
  settings.hTreshold = _hTreshold;
  settings.maxOnTime = _maxOnTime;
  settings.maxOffTime = _maxOffTime;
  settings.switchType = _switchType;
  writeStorage(_storagePointer, settings);
}

byte DHTSwitch::_readDeviceState () {
  
  // Relay on/off states may be inversed depending on physical connections
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
      aJson.addNumberToObject(moduleItem, "tTreshold", _tTreshold);
      aJson.addNumberToObject(moduleItem, "hTreshold", _tTreshold);
      aJson.addNumberToObject(moduleItem, "maxOnTime", _maxOnTime);
      aJson.addNumberToObject(moduleItem, "maxOffTime", _maxOffTime);
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

      moduleItemProperty = aJson.getObjectItem(moduleItem, "tTreshold");
      moduleItemProperty->valuefloat = _tTreshold;

      moduleItemProperty = aJson.getObjectItem(moduleItem, "hTreshold");
      moduleItemProperty->valuefloat = _hTreshold;

      moduleItemProperty = aJson.getObjectItem(moduleItem, "maxOnTime");
      moduleItemProperty->valueint = _maxOnTime;

      moduleItemProperty = aJson.getObjectItem(moduleItem, "maxOffTime");
      moduleItemProperty->valueint = _maxOffTime;

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

  if (settings->tTreshold < _sensor->getLowerBoundTemperature()) {
    return false;
  }
  
  if ((settings->tTreshold > _sensor->getUpperBoundTemperature()) && (settings->tTreshold != 65535)) {
    return false;
  }

  if (settings->hTreshold < _sensor->getLowerBoundHumidity()) {
    return false;
  }

  if ((settings->hTreshold > _sensor->getUpperBoundHumidity()) && (settings->hTreshold != 65535)) {
    return false;
  }

  if ((settings->maxOnTime < 0) || (settings->maxOffTime < 0)) {
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
  double newTTreshold = -1;
  double newHTreshold = -1;
  int newMaxOnTime = -1;
  int newMaxOffTime = -1;
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

  moduleItemProperty = aJson.getObjectItem(moduleItem,  "tTreshold");
  newTTreshold = moduleItemProperty->valuefloat;

  moduleItemProperty = aJson.getObjectItem(moduleItem,  "hTreshold");
  newHTreshold = moduleItemProperty->valuefloat;

  moduleItemProperty = aJson.getObjectItem(moduleItem,  "maxOnTime");
  newMaxOnTime = moduleItemProperty->valueint;

  moduleItemProperty = aJson.getObjectItem(moduleItem,  "maxOffTime");
  newMaxOffTime = moduleItemProperty->valueint;

  moduleItemProperty = aJson.getObjectItem(moduleItem,  "switchType");
  newSwitchType = moduleItemProperty->valueint;
  
  settings.driveMode = newDriveMode;
  settings.moduleState = newModuleState;
  settings.tTreshold = newTTreshold;
  settings.hTreshold = newHTreshold;
  settings.maxOnTime = newMaxOnTime;
  settings.maxOffTime = newMaxOffTime;
  settings.switchType = newSwitchType;

  
  if (!_validateSettings(&settings)) {
    return false;
  }
  
  _tTreshold = newTTreshold;
  _hTreshold = newHTreshold;
  _maxOnTime = newMaxOnTime;
  _maxOffTime = newMaxOffTime;
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

boolean DHTSwitch::_checkTreshold() {
  // It there's a treshold for temperature
  if (_tTreshold != 65535) {
    if ((_sensor->getTemperature() > _tTreshold) && (_switchType == 1)) {
      return true;
    }

    if ((_sensor->getTemperature() < _tTreshold) && (_switchType == 0)) {
      return true;
    }
  }
  
  if (_hTreshold != 65535) {
    if ((_sensor->getHumidity() > _hTreshold) && (_switchType == 1)) {
      return true;
    }

    if ((_sensor->getHumidity() < _hTreshold) && (_switchType == 0)) {
      return true;
    }
  }

  return false;
}

void DHTSwitch::loopDo() {
  int8_t deviceState;

  // If the module is on now
  if (_moduleState && (_driveMode == 0)) {
    deviceState = _readDeviceState();
    
    // If we are in 'active' state: treshold is crossed, need to switch the relay on
    if (_checkTreshold()) {
      // Reset the "off" counter when we cross the treshold
      if (_maxOffTime > 0) {
        _timeCounter = 0;
      }
  
      // If relay is off turn it on (only if we haven't already switched on)
      if ((deviceState == 0) && (_timeCounter == 0)) {
        
        // If we have a time limit then save a timestamp for later check
        if (_maxOnTime > 0) {
          _timeCounter = millis();
        }
  
        digitalWrite(_relayPin, SWITCH_RELAY_ON);
      } else {
        // If relay is on then check for a time limit
        if ((_maxOnTime > 0) && (abs(millis() - _timeCounter) > _maxOnTime)) {
          if (deviceState == 1) {
            digitalWrite(_relayPin, SWITCH_RELAY_OFF);
          }
        }
      }
    } else {
      // Reset "on" counter after we fell "below" the treshold
      if (_maxOnTime > 0) {
        _timeCounter = 0;
      }
  
      // If relay is on turn it off (only if we haven't already switched off)
      if ((deviceState == 1) && (_timeCounter == 0)) {
        
        // If we have a time limit then save a timestamp for later check
        if (_maxOffTime > 0) {
          _timeCounter = millis();
        }
  
        digitalWrite(_relayPin, SWITCH_RELAY_OFF);
      } else {
        // If relay is on then check for a time limit
        if ((_maxOffTime > 0) && (abs(millis() - _timeCounter) > _maxOffTime)) {
          if (deviceState == 0) {
            digitalWrite(_relayPin, SWITCH_RELAY_ON);
          }
        }
      }
      
    }
    // end of if checkTreshold
  }
  // end of if module is on
}