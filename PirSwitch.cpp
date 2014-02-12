#include "Arduino.h"
#include "HiveStorage.h"
#include "PirSwitch.h"
#include "SensorModule.h"
#include "aJson.h"
#include "AppContext.h"

const char PirSwitch::_moduleType[12] = "PirSwitch";

PirSwitch::PirSwitch(AppContext *context, const byte zone, byte moduleId, int storagePointer, boolean loadSettings, int8_t switchPin, int8_t lightPin) : 
  SensorModule(storagePointer, moduleId, zone),
  _switchPin(switchPin),
  _lightPin(lightPin),
  _context(context) {
  
  _stateChanged = true;
  _resetSettings();
  
  // DEBUG
  Serial.println(F("Init PirSwitch"));

  pinMode(_switchPin, INPUT);
  
  if (loadSettings) {
    _loadSettings();
  } else {
    // If there's no load flag then we haven't saved anything yet, so init the storage
    _saveSettings();
  }
  
  pinMode(_lightPin, OUTPUT);
  _previousSwitchState = _readSwitchState();

 

  if (!_moduleState) {
    digitalWrite(_lightPin, SWITCH_RELAY_OFF);
  } else {
  
    switch (_lightMode) {
      case 1:
        // If there's a manual "on" override
        digitalWrite(_lightPin, SWITCH_RELAY_ON);
        break;
      case 2:
        // If there's a manual "off" override
        digitalWrite(_lightPin, SWITCH_RELAY_OFF);
        break;
      case 0:
        // If there's auto switch mode
        _previousSwitchState ? digitalWrite(_lightPin, SWITCH_RELAY_ON) : digitalWrite(_lightPin, SWITCH_RELAY_OFF);
        break;
    }
    
    _previousLightState = _readLightState();
  }

  // DEBUG
  Serial.println(F("Finished PirSwitch init"));
}

byte PirSwitch::getStorageSize () {
  return sizeof(config_t);
}

void PirSwitch::_resetSettings() {
  _pirDelay = 10;
  _previousLightState = 0;
  _moduleState = 1;
  _lightMode = 0;
}

void PirSwitch::_loadSettings() {
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
    _lightMode = settings.lightMode;
    _moduleState = settings.moduleState;
    _pirDelay = settings.pirDelay;
  } else {
    // If unable to read then reset settings to default
    // and try write them back to fix the storage
    // (suppose the settings file was corrupt in some way)
    _resetSettings();
    _saveSettings();
  }

  _stateChanged = true;
}


void PirSwitch::_saveSettings() {
  config_t settings;
  settings.lightMode = _lightMode;
  settings.moduleState = _moduleState;
  settings.pirDelay = _pirDelay;
  writeStorage(_storagePointer, settings);
  
  //DEBUG
  Serial.println("Save module settings");
}

byte PirSwitch::_readSwitchState () {
  return digitalRead(_switchPin);
}

byte PirSwitch::_readLightState () {
  
  // Relay on/off states may be inversed depending on physical connections
  // So we take it into account and return simple values
  // 1 if the relay is on, 0 - if it's off

  if (digitalRead(_lightPin) == SWITCH_RELAY_ON) {
    return 1;
  } else {
    return 0;
  }
}

void PirSwitch::_turnLightOff() {
  if (_lightMode != 2) {
    _lightMode = 2;

    // Refresh the light state information
    _previousLightState = _readLightState();
    _stateChanged = true;
    _saveSettings();
  }
}

void PirSwitch::_turnLightOn() {
  if (_lightMode != 1) {
    _lightMode = 1;

    // Refresh the light state information
    _previousLightState = _readLightState();
    _stateChanged = true;
    _saveSettings();
  }
}

void PirSwitch::_turnLightAuto() {
  _lightMode = 0;

  // Check if the light state differs from switch state
  // to accomodate it in the loopDo method
  _switchState = _readSwitchState();
  _previousLightState = _readLightState();

  if (_switchState != _previousLightState) {
    _previousSwitchState = !_switchState;
  }

  _stateChanged = true;
  _saveSettings();
}

void PirSwitch::_pushNotify() {
  _context->pushNotify(_moduleId);
}

void PirSwitch::getJSONSettings() {
  // generate json settings only if something's changed
  // TODO: mark properties with read-only and read-write types

  if (_stateChanged) {

    aJsonObject *moduleItem = aJson.getArrayItem(*(_context->moduleCollection), _moduleId-1);
    aJsonObject *moduleItemProperty = aJson.getObjectItem(moduleItem, "moduleType");
    
    // If we have an empty JSON settings structure
    // then fill it with values

    if (strcmp(moduleItemProperty->valuestring, _moduleType) != 0) {    
      
      // TODO: Add prefixes to param names to mark readonly fields
      aJson.addStringToObject(moduleItem, "moduleType", _moduleType);
      aJson.addNumberToObject(moduleItem, "moduleState", _moduleState);
      aJson.addNumberToObject(moduleItem, "zoneId", _moduleZone);
      aJson.addNumberToObject(moduleItem, "switchState", _readSwitchState());
      aJson.addNumberToObject(moduleItem, "lightState", _readLightState());
      aJson.addNumberToObject(moduleItem, "lightMode", _lightMode);

      aJson.addNumberToObject(moduleItem, "pirDelay", _pirDelay);

    } else {
      // If we have an already initialized settings JSON structure
      // then just replace the values

      moduleItemProperty = aJson.getObjectItem(moduleItem, "moduleState");
      moduleItemProperty->valuebool = _moduleState;

      moduleItemProperty = aJson.getObjectItem(moduleItem, "switchState");
      moduleItemProperty->valueint = _readSwitchState();

      moduleItemProperty = aJson.getObjectItem(moduleItem, "lightState");
      moduleItemProperty->valueint = _readLightState();

      moduleItemProperty = aJson.getObjectItem(moduleItem, "lightMode");
      moduleItemProperty->valueint = _lightMode;

      moduleItemProperty = aJson.getObjectItem(moduleItem, "pirDelay");
      moduleItemProperty->valueint = _pirDelay;
    }

    _stateChanged = false;

  }
}

// TODO: if error, return settings object with error item
boolean PirSwitch::_validateSettings(config_t *settings) {
  if ((settings->lightMode < 0) || (settings->lightMode > 2)) {
    return false;
  }
  
  if ((settings->moduleState < 0) || (settings->moduleState > 1)) {
    return false;
  }

  // Check turn off delay after PIR switch is activated (in milliseconds)
  // Must be between 0 and 24 hours
  if ((settings->pirDelay < 0) || (settings->pirDelay > 255)) {
    return false;
  }
  
  return true;
}

boolean PirSwitch::setJSONSettings(aJsonObject *moduleItem) {
  config_t settings;
  aJsonObject *moduleItemProperty;
  // Initialiaze to invalid values so the validation works
  int8_t newModuleState = -1;
  int8_t newLightMode = -1;
  int8_t newPirDelay = -1;
  
  // Check for module type first
  moduleItemProperty = aJson.getObjectItem(moduleItem, "moduleType");
  if (strcmp(moduleItemProperty->valuestring, _moduleType) != 0) {
    return false;
  }

  moduleItemProperty = aJson.getObjectItem(moduleItem, "moduleState");
  newModuleState = moduleItemProperty->valuebool;
  
  moduleItemProperty = aJson.getObjectItem(moduleItem,  "lightMode");
  newLightMode = moduleItemProperty->valueint;

  moduleItemProperty = aJson.getObjectItem(moduleItem,  "pirDelay");
  newPirDelay = moduleItemProperty->valueint;
  
  settings.lightMode = newLightMode;
  settings.moduleState = newModuleState;
  settings.pirDelay = newPirDelay;
  
  if (!_validateSettings(&settings)) {
    return false;
  }
  
  if (_moduleState != newModuleState) {
    newModuleState ? turnModuleOn() : turnModuleOff();
  }
  
  if (_lightMode != newLightMode) {
    switch (newLightMode) {
      case 0:
        _turnLightAuto();
        break;
      case 1:
        _turnLightOn();
        break;
      case 2:
        _turnLightOff();
        break;
    }
  }
  
  return true;
}

void PirSwitch::turnModuleOff() {
  if (_moduleState) {
    digitalWrite(_lightPin, SWITCH_RELAY_OFF);
    _moduleState = false;
    _stateChanged = true;
    _saveSettings();
  }
}

void PirSwitch::turnModuleOn() {
  if (!_moduleState) {
    _switchState = digitalRead(_switchPin);
    
    // Check current operation mode (auto or manual override)
    if (_lightMode == 0) {
      _switchState ? digitalWrite(_lightPin, SWITCH_RELAY_ON) : digitalWrite(_lightPin, SWITCH_RELAY_OFF);
    } else {
      _lightMode == 1 ? digitalWrite(_lightPin, SWITCH_RELAY_ON) : digitalWrite(_lightPin, SWITCH_RELAY_OFF);
    }

    _moduleState = true;
    _stateChanged = true;
    _saveSettings();
  }
}

void PirSwitch::loopDo() {
  // If the module is on now
  if (_moduleState) {

    if (_lightMode == 1 && _previousLightState == 0) {
      // If we have manual "on" override mode
      digitalWrite(_lightPin, SWITCH_RELAY_ON);
      _previousLightState = 1;
    } else if (_lightMode == 2 && _previousLightState == 1) {
      // If we have manual "off" override mode
      digitalWrite(_lightPin, SWITCH_RELAY_OFF);
      _previousLightState = 0;
    }
        
    _switchState = _readSwitchState();

    if (_switchState == HIGH) {
      // If PIR switch is activated

      if (!_previousLightState) {
        digitalWrite(_lightPin, SWITCH_RELAY_ON);
        _previousLightState = _switchState;
        _stateChanged = true;
        _pushNotify();
      }

      // We start delay countdouwn only when switch is off
      // Otherwise we update counter value
      _delayCounter = millis();
    } else {
      // If PIR switch is off
      // Check counter value vs current time
      // _pirDelay value is in seconds so multiply it by 1000
      if ((abs(millis() - _delayCounter) >= _pirDelay * 1000) && (_previousLightState == HIGH)) {
        digitalWrite(_lightPin, SWITCH_RELAY_OFF);
        _previousLightState = _switchState;
        _delayCounter = 0;
        _stateChanged = true;
        _pushNotify();
      }
    }

  }
}
