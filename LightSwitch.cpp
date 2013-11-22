#include "Arduino.h"
#include "HiveStorage.h"
#include "LightSwitch.h"
#include "SensorModule.h"
#include "aJson.h"
#include "AppContext.h"
#include "MemoryFree.h"

const char LightSwitch::_moduleType[12] = "LightSwitch";

LightSwitch::LightSwitch(AppContext *context, const byte zone, byte moduleId, int storagePointer, boolean loadSettings, int8_t switchPin, int8_t lightPin) : 
  SensorModule(storagePointer, moduleId, zone),
  _switchPin(switchPin),
  _lightPin(lightPin),
  _context(context) {
  
  _stateChanged = true;
  _debounceTime = 20;
  _debounceCounter = 0;
  _previousSwitchState = 0;
  _previousLightState = 0;
  _moduleState = 1;
  _lightMode = 0;
  
  // DEBUG
  Serial.println(F("Init LightSwitch"));

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
    digitalWrite(_lightPin, LIGHTSWITCH_RELAY_OFF);
  } else {
  
    switch (_lightMode) {
      case 1:
        // If there's a manual "on" override
        digitalWrite(_lightPin, LIGHTSWITCH_RELAY_ON);
        break;
      case 2:
        // If there's a manual "off" override
        digitalWrite(_lightPin, LIGHTSWITCH_RELAY_OFF);
        break;
      case 0:
        // If there's auto switch mode
        _previousSwitchState ? digitalWrite(_lightPin, LIGHTSWITCH_RELAY_ON) : digitalWrite(_lightPin, LIGHTSWITCH_RELAY_OFF);
        break;
    }
    
    _previousLightState = _readLightState();
  }

  // DEBUG
  Serial.println(F("Finished LightSwitch init"));
}

byte LightSwitch::getStorageSize () {
  return sizeof(config_t);
}

void LightSwitch::_loadSettings() {
  config_t settings;

  readStorage(_storagePointer, settings);
  _lightMode = settings.lightMode;
  _moduleState = settings.moduleState;
  _stateChanged = true;
}

void LightSwitch::_saveSettings() {
  config_t settings;
  settings.lightMode = _lightMode;
  settings.moduleState = _moduleState;
  writeStorage(_storagePointer, settings);
}

byte LightSwitch::_readSwitchState () {
  return digitalRead(_switchPin);
}

byte LightSwitch::_readLightState () {
  
  // Relay on/off states may be inversed depending on physical connections
  // So we take it into account and return simple values
  // 1 if the relay is on, 0 - if it's off

  if (digitalRead(_lightPin) == LIGHTSWITCH_RELAY_ON) {
    return 1;
  } else {
    return 0;
  }
}

void LightSwitch::_turnLightOff() {
  if (_lightMode != 2) {
    _lightMode = 2;

    // Refresh the light state information
    _previousLightState = _readLightState();
    _stateChanged = true;
    _saveSettings();
  }
}

void LightSwitch::_turnLightOn() {
  if (_lightMode != 1) {
    _lightMode = 1;

    // Refresh the light state information
    _previousLightState = _readLightState();
    _stateChanged = true;
    _saveSettings();
  }
}

void LightSwitch::_turnLightAuto() {
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

void LightSwitch::_pushNotify() {
  _context->pushNotify(_moduleId);
}

void LightSwitch::getJSONSettings() {
  // generate json settings only if something's changed
  // TODO: mark properties with read-only and read-write types

  if (_stateChanged) {

    aJsonObject *moduleItem = aJson.getArrayItem(*(_context->moduleCollection), _moduleId-1);
    aJsonObject *moduleItemProperty = aJson.getObjectItem(moduleItem, "moduleType");
    
    // If we have an empty JSON settings structure
    // then fill it with values

    if (strcmp(moduleItemProperty->valuestring, _moduleType) != 0) {    
      
      // TODO: Add prefixes to param names to mark readonly fields
      aJson.addNumberToObject(moduleItem, "id", _moduleId);
      aJson.addStringToObject(moduleItem, "moduleType", _moduleType);
      aJson.addNumberToObject(moduleItem, "moduleState", _moduleState);
      aJson.addNumberToObject(moduleItem, "zoneId", _moduleZone);
      aJson.addNumberToObject(moduleItem, "switchState", _readSwitchState());
      aJson.addNumberToObject(moduleItem, "lightState", _readLightState());
      aJson.addNumberToObject(moduleItem, "lightMode", _lightMode);

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
    }

    _stateChanged = false;

  }
}

// TODO: if error, return settings object with error item
boolean LightSwitch::_validateSettings(config_t *settings) {
  if ((settings->lightMode < 0) || (settings->lightMode > 2)) {
    return false;
  }
  
  if ((settings->moduleState < 0) || (settings->moduleState > 1)) {
    return false;
  }
  
  return true;
}

boolean LightSwitch::setJSONSettings(aJsonObject *moduleItem) {
  config_t settings;
  aJsonObject *moduleItemProperty;
  // Initialiaze to invalid values so the validation works
  int8_t newModuleState = -1;
  int8_t newLightMode = -1;
  
  // Check for module type first
  moduleItemProperty = aJson.getObjectItem(moduleItem, "moduleType");
  if (strcmp(moduleItemProperty->valuestring, _moduleType) != 0) {
    return false;
  }

  moduleItemProperty = aJson.getObjectItem(moduleItem, "moduleState");
  newModuleState = moduleItemProperty->valuebool;
  
  moduleItemProperty = aJson.getObjectItem(moduleItem,  "lightMode");
  newLightMode = moduleItemProperty->valueint;
  
  settings.lightMode = newLightMode;
  settings.moduleState = newModuleState;
  
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

void LightSwitch::turnModuleOff() {
  if (_moduleState) {
    digitalWrite(_lightPin, LIGHTSWITCH_RELAY_OFF);
    _moduleState = false;
    _stateChanged = true;
  }
}

void LightSwitch::turnModuleOn() {
  if (!_moduleState) {
    _switchState = digitalRead(_switchPin);
    
    // Check current operation mode (auto or manual override)
    if (_lightMode == 0) {
      _switchState ? digitalWrite(_lightPin, LIGHTSWITCH_RELAY_ON) : digitalWrite(_lightPin, LIGHTSWITCH_RELAY_OFF);
    } else {
      _lightMode == 1 ? digitalWrite(_lightPin, LIGHTSWITCH_RELAY_ON) : digitalWrite(_lightPin, LIGHTSWITCH_RELAY_OFF);
    }

    _moduleState = true;
    _stateChanged = true;
  }
}

void LightSwitch::loopDo() {
  if (_moduleState) {
    if (_lightMode == 1 && (_previousLightState == 0)) {  // If there's a manual "on" override
      digitalWrite(_lightPin, LIGHTSWITCH_RELAY_ON);
      _previousLightState = 1;
    }
    
    if (_lightMode == 2 && (_previousLightState == 1)) { // If there's a manual "off" override
      digitalWrite(_lightPin, LIGHTSWITCH_RELAY_OFF);
      _previousLightState = 0;
    }
    
    if (_lightMode == 0) {
      _switchState = _readSwitchState();

      if (_previousSwitchState != _switchState) {
        _debounceCounter = millis();
        _previousSwitchState = _switchState;
      }
      
      if ((_debounceCounter > 0) && (millis() - _debounceCounter) > _debounceTime) { // we reach here if the switch is stable for the debounce time
        _debounceCounter = 0;

        if (_switchState != _previousLightState) { 
          // switch the light if previous light state differs and switch is debounced
          _switchState ? digitalWrite(_lightPin, LIGHTSWITCH_RELAY_ON) : digitalWrite(_lightPin, LIGHTSWITCH_RELAY_OFF);
          
          _previousLightState = _readLightState();

          // Notify remote server
          _pushNotify();
          _stateChanged = true;
        }
        // end switch after debounce
      }
      // end check if debounced
    }
    // end lightMode == auto
  }
}
