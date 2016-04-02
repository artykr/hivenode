/*
  Circuit for a switch and a relay is driven by two relays:

  switch relay:    normally open relay for switching a device on and off depending on switch position
  fallback relay:  normally closed relay provides parallel connection to the switch so it can be operated manually
*/

#include "Arduino.h"
#include "HiveStorage.h"
#include "FallbackSwitch.h"
#include "SensorModule.h"
#include "aJson.h"
#include "AppContext.h"
#include "MemoryFree.h"
#include "HiveUtils.h"

const char FallbackSwitch::_moduleType[15] = "FallbackSwitch";

FallbackSwitch::FallbackSwitch(AppContext *context, const byte zone, byte moduleId, int storagePointer, boolean loadSettings, int8_t switchPin, int8_t devicePin, int8_t fallbackPin, boolean usePullup) :
  SensorModule(storagePointer, moduleId, zone),
  _switchPin(switchPin),
  _devicePin(devicePin),
  _fallbackPin(fallbackPin),
  _usePullup(usePullup),
  _context(context) {

  _stateChanged = true;
  _resetSettings();

  // DEBUG
  debugPrint(F("FBS: Init FallbackSwitch"));

  if (_usePullup) {
    // Don't use it on pin 13
    pinMode(_switchPin, INPUT_PULLUP);
  } else {
    pinMode(_switchPin, INPUT);
  }

  if (loadSettings) {
    _loadSettings();
  } else {
    // If there's no load flag then we haven't saved anything yet, so init the storage
    _saveSettings();
  }

  pinMode(_devicePin, OUTPUT);
  pinMode(_fallbackPin, OUTPUT);
  _previousSwitchState = _readSwitchState();

  if (!_moduleState) {
    digitalWrite(_devicePin, FALLBACKSWITCH_RELAY_OFF);
    digitalWrite(_fallbackPin, FALLBACKSWITCH_RELAY_OFF);
  } else {
    digitalWrite(_fallbackPin, FALLBACKSWITCH_RELAY_ON);
    switch (_lightMode) {
      case 1:
        // If there's a manual "on" override
        digitalWrite(_devicePin, FALLBACKSWITCH_RELAY_ON);
        break;
      case 2:
        // If there's a manual "off" override
        digitalWrite(_devicePin, FALLBACKSWITCH_RELAY_OFF);
        break;
      case 0:
        // If there's auto switch mode
        _previousSwitchState ? digitalWrite(_devicePin, FALLBACKSWITCH_RELAY_ON) : digitalWrite(_devicePin, FALLBACKSWITCH_RELAY_OFF);
        break;
    }

    _previousLightState = _readLightState();
  }

  // DEBUG
  debugPrint(F("FBS: Finished FallbackSwitch init"));
}

void FallbackSwitch::_resetSettings() {
  _debounceTime = 20;
  _debounceCounter = 0;
  _previousSwitchState = 0;
  _previousLightState = 0;
  _moduleState = 1;
  _lightMode = 0;
  _switchCount = 0;
}

byte FallbackSwitch::getStorageSize() {
  return sizeof(config_t);
}

void FallbackSwitch::_loadSettings() {
  boolean isLoaded = false;

  //DEBUG
  debugPrint(F("FBS: Loading module settings"));

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
  } else {
    // If unable to read then reset settings to default
    // and try write them back to fix the storage
    // (suppose the settings file was corrupt in some way)
    _resetSettings();
    _saveSettings();
  }

  _stateChanged = true;
}

void FallbackSwitch::_saveSettings() {
  //DEBUG
  debugPrint(F("FBS: Saving module settings"));

  config_t settings;

  settings.lightMode = _lightMode;
  settings.moduleState = _moduleState;
  writeStorage(_storagePointer, settings);
}

byte FallbackSwitch::_readSwitchState () {
  boolean state = digitalRead(_switchPin);

  if (_usePullup) {
    return !state;
  } else {
    return state;
  }
}

byte FallbackSwitch::_readLightState () {

  // Relay on/off states may be inversed depending on physical connections
  // So we take it into account and return simple values
  // 1 if the relay is on, 0 - if it's off

  if (digitalRead(_devicePin) == FALLBACKSWITCH_RELAY_ON) {
    return 1;
  } else {
    return 0;
  }
}

void FallbackSwitch::_turnLightOff() {
  if (_lightMode != 2) {
    _lightMode = 2;

    // Refresh the light state information
    _previousLightState = _readLightState();
    _stateChanged = true;
    _saveSettings();
  }
}

void FallbackSwitch::_turnLightOn() {
  if (_lightMode != 1) {
    _lightMode = 1;

    // Refresh the light state information
    _previousLightState = _readLightState();
    _stateChanged = true;
    _saveSettings();
  }
}

void FallbackSwitch::_turnLightAuto() {
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

void FallbackSwitch::_pushNotify() {
  _context->pushNotify(moduleId);
}

void FallbackSwitch::getJSONSettings() {
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
boolean FallbackSwitch::_validateSettings(config_t *settings) {
  if ((settings->lightMode < 0) || (settings->lightMode > 2)) {
    return false;
  }

  if ((settings->moduleState < 0) || (settings->moduleState > 1)) {
    return false;
  }

  return true;
}

boolean FallbackSwitch::setJSONSettings(aJsonObject *moduleItem) {
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

void FallbackSwitch::turnModuleOff() {
  if (_moduleState) {
    digitalWrite(_devicePin, FALLBACKSWITCH_RELAY_OFF);
    digitalWrite(_fallbackPin, FALLBACKSWITCH_RELAY_OFF);
    _moduleState = false;
    _stateChanged = true;
    _saveSettings();
  }
}

void FallbackSwitch::turnModuleOn() {
  if (!_moduleState) {
    _switchState = digitalRead(_switchPin);
    digitalWrite(_fallbackPin, FALLBACKSWITCH_RELAY_ON);

    // Check current operation mode (auto or manual override)
    if (_lightMode == 0) {
      _switchState ? digitalWrite(_devicePin, FALLBACKSWITCH_RELAY_ON) : digitalWrite(_devicePin, FALLBACKSWITCH_RELAY_OFF);
    } else {
      _lightMode == 1 ? digitalWrite(_devicePin, FALLBACKSWITCH_RELAY_OFF) : digitalWrite(_devicePin, FALLBACKSWITCH_RELAY_OFF);
    }

    _moduleState = true;
    _stateChanged = true;
    _saveSettings();
  }
}

void FallbackSwitch::loopDo() {
  // If the module is on now
  if (_moduleState) {
    // If the module is in the override mode, user can bring it back
    // to the auto mode by double-flipping the wall switch.
    // So we first check if we have a double flip.
    if (_switchCount >= 2) {
      // Set module mode to auto
      _lightMode = 0;
      _turnLightAuto();
      _switchCount = 0;
    } else if (_lightMode == 1 && _previousLightState == 0) {
      // If we have manual "on" override mode
      digitalWrite(_devicePin, FALLBACKSWITCH_RELAY_ON);
      _previousLightState = 1;
    } else if (_lightMode == 2 && _previousLightState == 1) {
      // If we have manual "off" override mode
      digitalWrite(_devicePin, FALLBACKSWITCH_RELAY_OFF);
      _previousLightState = 0;
    }

    // Let's debounce the switch

    _switchState = _readSwitchState();

    if (_previousSwitchState != _switchState) {
      _debounceCounter = millis();
      _previousSwitchState = _switchState;
    }

    if ((_debounceCounter > 0) && (millis() - _debounceCounter) > _debounceTime) {
      // we reach here if the switch is stable for the debounce time
      _debounceCounter = 0;

      if (_switchState != _previousLightState) {
        if (_lightMode >= 1) {
          // If there's a manual override of whatever kind
          // count switch flips while in override mode
           _switchCount++;
        }
        // If we are in auto switching mode
        if (_lightMode == 0) {
          // switch the light if previous light state differs and switch is debounced
          _switchState ? digitalWrite(_devicePin, FALLBACKSWITCH_RELAY_ON) : digitalWrite(_devicePin, FALLBACKSWITCH_RELAY_OFF);

          _previousLightState = _readLightState();

          // Notify remote server
          _pushNotify();
          _stateChanged = true;
          _switchCount = 0;
        }
      }
      // end switch after debounce
    }
    // end check if debounced
  }
}
