#include "Arduino.h"
#include "HiveStorage.h"
#include "FloorHeater.h"
#include "SensorModule.h"
#include "aJson.h"
#include "AppContext.h"
#include "MemoryFree.h"
#include "avr/io.h"
#include "avr/interrupt.h"
#include "HiveUtils.h"
#include "ds3231.h"

const char FloorHeater::_moduleType[15] = "FloorHeater";

FloorHeater::FloorHeater(AppContext *context, OWTSensor *sensor, const byte zone, byte moduleId, int storagePointer, uint8_t devicePin, uint8_t timer, boolean loadSettings, float tMin, float tMax) :
  SensorModule(storagePointer, moduleId, zone),
  _tMin(tMin),
  _tMax(tMax),
  _devicePin(devicePin),
  _timer(timer),
  _sensor(sensor),
  _context(context) {

  _stateChanged = true;

  // Throw in some defaults: kP = 400, kI = 0.001, output limits 0..10000 ms, time step = 1000 ms
  _controller = new PID(400.0, 0.001, 0, &_input, &_output, 0, 10000, &_setpoint, 1000, true);
  _resetSettings();

  _windowStartTime = millis();

  for(uint8_t i = 0; i < 7; i++) {
    for (uint8_t j = 0; j < 3; j++) {
      for (uint8_t k = 0; k < 3; k++) {
        _schedule[i][j][k] = 0;
      }
    }
  }

  // DEBUG
  debugPrint(F("Init FH"));

  if (loadSettings) {
    _loadSettings();
  } else {
    // If there's no load flag then we haven't saved anything yet, so init the storage
    _saveSettings();
  }

  // DEBUG
  debugPrint(F("FH: Settings loaded/saved"));

  pinMode(_devicePin, OUTPUT);
  digitalWrite(_devicePin, FLOORHEATER_RELAY_OFF);

  // DEBUG
  debugPrint(F("FH: Init ISR timer"));

  switch (_timer) {
    case 3:
      _initTimer3();
      break;
    case 4:
      _initTimer4();
      break;
    case 5:
      _initTimer5();
      break;
  }
}

void FloorHeater::_initTimer3() {
  cli();          // disable global interrupts

  // DEBUG
  debugPrint(F("FH: Global interrupts disabled"));

  TCCR3A = 0;     // set entire TCCRnA register to 0
  TCCR3B = 0;     // same for TCCRnB

  // set compare match register to desired timer count:
  OCR3A = 29999;

  // turn on CTC mode:
  TCCR3B |= (1 << WGM32);

  // Set CSn1 bit for 8 prescaler:
  TCCR3B |= (1 << CS31);

  // enable timer compare interrupt:
  TIMSK3 |= (1 << OCIE3A);
  sei();          // enable global interrupts

  // DEBUG
  debugPrint(F("FH: Init ISR timer 3"));
}

void FloorHeater::_initTimer4() {
  cli();          // disable global interrupts
  TCCR4A = 0;     // set entire TCCRnA register to 0
  TCCR4B = 0;     // same for TCCRnB

  // set compare match register to desired timer count:
  OCR4A = 29999;

  // turn on CTC mode:
  TCCR4B |= (1 << WGM42);

  // Set CSn1 bit for 8 prescaler:
  TCCR4B |= (1 << CS41);

  // enable timer compare interrupt:
  TIMSK4 |= (1 << OCIE4A);
  sei();          // enable global interrupts
}

void FloorHeater::_initTimer5() {
  cli();          // disable global interrupts
  TCCR5A = 0;     // set entire TCCRnA register to 0
  TCCR5B = 0;     // same for TCCRnB

  // set compare match register to desired timer count:
  OCR5A = 29999;

  // turn on CTC mode:
  TCCR5B |= (1 << WGM52);

  // Set CSn1 bit for 8 prescaler:
  TCCR5B |= (1 << CS51);

  // enable timer compare interrupt:
  TIMSK5 |= (1 << OCIE5A);
  sei();          // enable global interrupts
}

void FloorHeater::_resetSettings() {
  _moduleState = 1;
  _deviceState = 0;
  _driveMode = 2; // device is off
  _output = 0;
  _setpoint = 28.0;
  _resetTuning();
  memset(_schedule, 0, sizeof(_schedule));
}

void FloorHeater::_resetTuning() {
  _controller->setKs(400.0, 0.001, 0);
  _stableTime = 500000;
  _lastTuning = 0;
}

byte FloorHeater::getStorageSize() {
  return sizeof(config_t);
}

void FloorHeater::_loadSettings() {
  boolean isLoaded = false;

  config_t settings;

  // Try to read storage and validate
  if (readStorage(_storagePointer, settings) > 0) {
    if (_validateSettings(&settings)) {
      isLoaded = true;
    }
  }

  if (isLoaded) {
    _driveMode = settings.driveMode;
    _setpoint = settings.setpoint;
    _controller->setKs(settings.kP, settings.kI, 0);
    _stableTime = settings.stableTime;
    _moduleState = settings.moduleState;
    _lastTuning = settings.lastTuning;

    for(uint8_t i = 0; i < 7; i++) {
      for (uint8_t j = 0; j < 3; j++) {
        for (uint8_t k = 0; k < 3; k++) {
          _schedule[i][j][k] = settings.schedule[i][j][k];
        }
      }
    }

  } else {
    // If unable to read then reset settings to default
    // and try write them back to fix the storage
    // (suppose the settings file was corrupt in some way)
    _resetSettings();
    _saveSettings();
  }

  _stateChanged = true;
}

void FloorHeater::_saveSettings() {

  config_t settings;

  settings.driveMode = _driveMode;
  settings.moduleState = _moduleState;
  settings.setpoint = _setpoint;
  settings.kP = _controller->getKp();
  settings.kI = _controller->getKi();
  settings.stableTime = _stableTime;
  settings.lastTuning = _lastTuning;

  for(uint8_t i = 0; i < 7; i++) {
    for (uint8_t j = 0; j < 3; j++) {
      for (uint8_t k = 0; k < 3; k++) {
        settings.schedule[i][j][k] = _schedule[i][j][k];
      }
    }
  }

  writeStorage(_storagePointer, settings);
}

// Turn off the device manually. Module is still on.
void FloorHeater::_turnDeviceOff() {
  _driveMode = 2;
  _deviceState = 0;
  digitalWrite(_devicePin, FLOORHEATER_RELAY_OFF);
  _stateChanged = true;
  _saveSettings();
}

// Manually turn the device on with current setpoint
void FloorHeater::_turnDeviceOn() {
  _driveMode = 1;
  _deviceState = 1;
  _stateChanged = true;
  _saveSettings();
}

// Turn on schedule mode
void FloorHeater::_turnDeviceBySchedule() {
  _driveMode = 0;
  _deviceState = 1;
  _stateChanged = true;
  _saveSettings();
}

void FloorHeater::_turnDeviceTuning() {
  _deviceState = 2;
  _controller->initPITuning(60, 0.4);
  _stateChanged = true;

  //DEBUG
  debugPrint(F("FH: Tuning mode ON"));
}

void FloorHeater::_pushNotify() {
  _context->pushNotify(moduleId);
}

void FloorHeater::getJSONSettings() {
  // generate json settings only if something's changed
  // TODO: mark properties with read-only and read-write types

  if (_stateChanged) {

    aJsonObject *moduleItem = aJson.getArrayItem(*(_context->moduleCollection), moduleId-1);
    aJsonObject *moduleItemProperty = aJson.getObjectItem(moduleItem, "moduleType");

    // Buffer for UL to char conversion
    char buffer[10];

    // If we have an empty JSON settings structure
    // then fill it with values

    if (strcmp(moduleItemProperty->valuestring, _moduleType) != 0) {

      // TODO: Add prefixes to param names to mark readonly fields
      aJson.addStringToObject(moduleItem, "moduleType", _moduleType);
      aJson.addNumberToObject(moduleItem, "moduleState", _moduleState);
      aJson.addNumberToObject(moduleItem, "zoneId", _moduleZone);
      aJson.addNumberToObject(moduleItem, "driveMode", _driveMode);
      aJson.addNumberToObject(moduleItem, "deviceState", _deviceState);
      aJson.addNumberToObject(moduleItem, "setpoint", _setpoint);
      aJson.addNumberToObject(moduleItem, "t", _input);
      aJson.addNumberToObject(moduleItem, "tMin", _tMin);
      aJson.addNumberToObject(moduleItem, "tMax", _tMax);

      itoa(_lastTuning, buffer, 10);
      aJson.addStringToObject(moduleItem, "lastTuning", buffer);

      moduleItemProperty = aJson.createArray();
      aJsonObject *day, *period;

      for(uint8_t i = 0; i < 7; i++) {
        aJson.addItemToArray(moduleItemProperty, day = aJson.createArray());

        for (uint8_t j = 0; j < 3; j++) {
          aJson.addItemToArray(day, period = aJson.createObject());
          aJson.addNumberToObject(period, "start", _schedule[i][j][0]);
          aJson.addNumberToObject(period, "end", _schedule[i][j][1]);
          aJson.addNumberToObject(period, "t", _schedule[i][j][2]);
        }
      }

      aJson.addItemToObject(moduleItem, "schedule", moduleItemProperty);

    } else {
      // If we have an already initialized settings JSON structure
      // then just replace the values

      moduleItemProperty = aJson.getObjectItem(moduleItem, "moduleState");
      moduleItemProperty->valuebool = _moduleState;

      moduleItemProperty = aJson.getObjectItem(moduleItem, "driveMode");
      moduleItemProperty->valueint = _driveMode;

      moduleItemProperty = aJson.getObjectItem(moduleItem, "deviceState");
      moduleItemProperty->valueint = _deviceState;

      moduleItemProperty = aJson.getObjectItem(moduleItem, "setpoint");
      moduleItemProperty->valuefloat = _setpoint;

      moduleItemProperty = aJson.getObjectItem(moduleItem, "lastTuning");

      itoa(_lastTuning, buffer, 10);
      moduleItemProperty->valuestring = buffer;

      moduleItemProperty = aJson.getObjectItem(moduleItem, "t");
      moduleItemProperty->valuefloat = _input;

      moduleItemProperty = aJson.getObjectItem(moduleItem, "schedule");

      aJsonObject *day, *period, *item;

      for(uint8_t i = 0; i < 7; i++) {
        day = moduleItemProperty->next;

        for (uint8_t j = 0; j < 3; j++) {
          period = day->next;
          item = aJson.getObjectItem(period, "start");
          _schedule[i][j][0] = item->valueint;
          item = aJson.getObjectItem(period, "end");
          _schedule[i][j][1] = item->valueint;
          item = aJson.getObjectItem(period, "t");
          _schedule[i][j][2] = item->valueint;
        }
      }
    }

    _stateChanged = false;

  }
}

boolean FloorHeater::_validateSettings(config_t *settings) {

  int startH, startM, endH, endM;

  if ((settings->moduleState < 0) || (settings->moduleState > 1)) {
    return false;
  }

  if ((settings->driveMode < 0) || (settings->driveMode > 2)) {
    return false;
  }

  if ((settings->setpoint < _tMin) || (settings->setpoint > _tMax)) {
    return false;
  }

  if ((settings->kP == 0) || (settings->kI == 0)) {
    return false;
  }

  if (settings->stableTime == 0) {
    return false;
  }

  for(uint8_t i = 0; i < 7; i++) {
    for (uint8_t j = 0; j < 3; j++) {
        settings->schedule[i][j][0];

        startH = highByte(settings->schedule[i][j][0]);
        startM = lowByte(settings->schedule[i][j][0]);

        endH = highByte(settings->schedule[i][j][1]);
        endM = lowByte(settings->schedule[i][j][1]);

        if ((startH == 0) && (startM == 0) && (endH == 0) && (endM == 0)) {
          continue;
        }

        // The heating time can't be less than an hour
        // The end time should be greater than the start
        if (startH <= endH) {
          return false;
        }

        // 23 is the maximum hour value
        if ((startH > 23) || (endH > 23)) {
          return false;
        }

        if ((startM > 59) || (endM > 59)) {
          return false;
        }

        // The heating time can't be less than an hour
        if ((endH - startH == 1) && (endM - startM < 0)) {
          return false;
        }

        if ((settings->schedule[i][j][1] < _tMin) || (settings->schedule[i][j][1] > _tMax)) {
          return false;
        }
    }
  }

  return true;
}

boolean FloorHeater::setJSONSettings(aJsonObject *moduleItem) {
  config_t settings;
  aJsonObject *moduleItemProperty;
  // Initialiaze to invalid values so the validation works
  int8_t newModuleState = -1;
  int8_t newDriveMode = -1;
  int8_t newSetpoint = -1;
  int8_t doTuning = -1;
  int8_t resetTuning = -1;

  // Check for module type first
  moduleItemProperty = aJson.getObjectItem(moduleItem, "moduleType");
  if (strcmp(moduleItemProperty->valuestring, _moduleType) != 0) {
    return false;
  }

  moduleItemProperty = aJson.getObjectItem(moduleItem, "moduleState");
  newModuleState = moduleItemProperty->valuebool;

  moduleItemProperty = aJson.getObjectItem(moduleItem,  "driveMode");
  newDriveMode = moduleItemProperty->valueint;

  moduleItemProperty = aJson.getObjectItem(moduleItem,  "setpoint");
  newSetpoint = moduleItemProperty->valuefloat;

  moduleItemProperty = aJson.getObjectItem(moduleItem, "doTuning");
  doTuning = moduleItemProperty->valuebool;

  moduleItemProperty = aJson.getObjectItem(moduleItem, "resetTuning");
  resetTuning = moduleItemProperty->valuebool;

  settings.driveMode = newDriveMode;
  settings.moduleState = newModuleState;
  settings.setpoint = newSetpoint;

  moduleItemProperty = aJson.getObjectItem(moduleItem, "schedule");

  aJsonObject *day, *period, *item;

  for(uint8_t i = 0; i < 7; i++) {
    day = moduleItemProperty->next;

    for (uint8_t j = 0; j < 3; j++) {
      period = day->next;
      item = aJson.getObjectItem(period, "start");
      settings.schedule[i][j][0] = item->valueint;
      item = aJson.getObjectItem(period, "end");
      settings.schedule[i][j][1] = item->valueint;
      item = aJson.getObjectItem(period, "t");
      settings.schedule[i][j][2] = item->valueint;
    }
  }

  if (!_validateSettings(&settings)) {
    return false;
  }

  if (_moduleState != newModuleState) {
    newModuleState ? turnModuleOn() : turnModuleOff();
  }

  if (_driveMode != newDriveMode) {
    switch (newDriveMode) {
      case 0:
        _turnDeviceBySchedule();
        break;
      case 1:
        _turnDeviceOn();
        break;
      case 2:
        _turnDeviceOff();
        break;
    }
  }

  if (resetTuning) {
    _resetTuning();
    _stateChanged = true;
  }

  if (doTuning == 1) {
    _turnDeviceTuning();
  }

  return true;
}

void FloorHeater::turnModuleOff() {
  if (_moduleState) {
    digitalWrite(_devicePin, FLOORHEATER_RELAY_OFF);
    _moduleState = false;
    _stateChanged = true;
    _saveSettings();
  }
}

void FloorHeater::turnModuleOn() {
  if (!_moduleState) {
    _moduleState = true;
    _stateChanged = true;
    _saveSettings();
  }
}

void FloorHeater::handleInterrupt() {

  if (_driveMode < 2) {
    unsigned long now = millis();

    if (timeDiff(_windowStartTime) > _OutputWindowSize) {
      _windowStartTime += _OutputWindowSize;
    }

    if ((timeDiff(_windowStartTime) < _outputTime) && (_outputTime > 150)) {
      digitalWrite(_devicePin, FLOORHEATER_RELAY_ON);
    } else {
      digitalWrite(_devicePin, FLOORHEATER_RELAY_ON);
    }
  }
}

// Check the schedule array if it's time to drive the heater
boolean FloorHeater::_checkSchedule() {
  // Get current time from RTC
  byte h = 0;
  byte m = 0;

  DS3231_get(&_time);

  // Add stable
  byte mPredict = _time.min + _stableTime / 60000;
  byte hPredict = _time.hour;

  if (mPredict >= 60) {
    mPredict -= 60;
    hPredict++;
  }

  int hm = word(_time.hour, _time.min);
  int hmPredict = word(hPredict, mPredict);

  for (uint8_t j = 0; j < 3; j++) {
    if (_schedule[_time.wday][j][0] != _schedule[_time.wday][j][1] && hmPredict >= _schedule[_time.wday][j][0] && hm <= _schedule[_time.wday][j][1]) {
      return true;
    }
  }

  return false;
}

void FloorHeater::loopDo() {
  if (_moduleState) {
    if (timeDiff(_lastControlTime) > _ControlTime) {
      _input = _sensor->getTemperature();

      // DEBUG
      debugPrint(F("FH: Current t: "), false);
      debugPrint(_input);

      // Tuning mode
      if (_deviceState == 2) {
        boolean finished = _controller->doPITuning();
        if (finished) {
          _deviceState = 0;
          _stableTime = _controller->getStableTime();
          _stateChanged = true;
        } else {
          return;
        }
      }

      boolean doControl = false;

      // Manual setpoint
      if (_driveMode == 1) {
        doControl = true;
      }

      // Setpoint by schedule
      if (_driveMode == 0 && _checkSchedule()) {
        doControl = true;
      }

      // Run the controller if we need to
      if (doControl) {
        _controller->doControl();
        _outputTime = _output;
      }
    }
  }
}
