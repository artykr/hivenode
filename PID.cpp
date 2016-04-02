#include "PID.h"
#include "HiveUtils.h"

// Parts from http://www.mstarlabs.com/apeng/techniques/pidsoftw.html

PID::PID (float kP, float kI, float kD, float *input, float *output, float limitMin, float limitMax, float *setpoint, uint16_t timeStep, boolean direction) :
  _input(input),
  _output(output),
  _limitMin(limitMin),
  _limitMax(limitMax),
  _setpoint(setpoint),
  _timeStep(timeStep) {

  setKs(kP, kI, kD);

  *_output = constrain(*_output, _limitMin, _limitMax);
  resetCalc();
  _previousCalcTime = millis() - _timeStep;

  // TRUE - means direct, FALSE - reverse
  _direction = true;
  setControlDirection(_direction);
  _useFilter = false;
}

// Sets PID coefs with respect to the sample time
// Since sample time is equal for every control loop
// we can put it "inside" the coefs instead of integral/derivative equations
void PID::setKs(float kP, float kI, float kD) {

  _kP = kP;
  _kI = kI * (float)_timeStep;
  _kD = kD / (float)_timeStep;

}

float PID::getKp() {
  return _kP;
}

float PID::getKi() {
  return _kI;
}

float PID::getKd() {
  return _kD;
}

// Change the sample time on the go
// keeping in mind we've built the sample time into the coefs
void PID::setTimeStep(uint16_t timeStep) {
  if (timeStep > 0)
  {
    float ratio  = (float)timeStep / (float)_timeStep;
    _kI *= ratio;
    _kD /= ratio;
    _timeStep = timeStep;
  }
}

// If we're back from manual mode we need a reset
void PID::resetCalc() {
  _integralTerm = *_output;
  _previousInput = *_input;
  _integralTerm = constrain(_integralTerm, _limitMin, _limitMax);
}

// Limit the controller output so it doesn't "overrun" at limits
void PID::setOutputLimits(float limitMin, float limitMax) {
  _limitMax = limitMax;
  _limitMin = limitMin;
  _constrainOutput();
}

// Apply the output limits
void PID::_constrainOutput() {
  float old_output = *_output;
  *_output = constrain(*_output, _limitMin, _limitMax);

  if (*_output != old_output) {
    _adjustIntegralTerm();
  }
}

// https://bitbucket.org/osrf/drcsim/issue/92/request-to-provide-integral-tie-back
void PID::_adjustIntegralTerm() {
  // Apply a tieback if output is cut by limits
  _integralTerm = *_output - _kP * _error + _kD * (*_input - _previousInput);
}

// Reverse controller direction
void PID::setControlDirection(boolean direction) {
  if (direction != _direction) {
    _kP = 0 - _kP;
    _kI = 0 - _kI;
    _kD = 0 - _kD;
    _direction = direction;
  }
}

// Calculate PID output
boolean PID::doControl() {
  unsigned long now = millis();

  // TODO: fix millis reset at 55-th day
  if (now - _previousCalcTime >= _timeStep) {
      // Use filter
      // Replaces _input value with filteres if enabled
      _doFilter();

      // Compute error, integral and dericative terms
      _error = *_setpoint - *_input;
      _integralTerm += (_kI * _error);
      _integralTerm = constrain(_integralTerm, _limitMin, _limitMax);
      float derivativeTerm = _kD * (*_input - _previousInput);

      *_output = _kP * _error + _integralTerm - derivativeTerm;

      // DEBUG
      debugPrint(*_output, false);
      debugPrint(";", false);

      _constrainOutput();

      _previousInput = *_input;
      _previousCalcTime = now;

      // We have a new output value
      return true;
  } else {
    // The time for a clac hasn't come yet
    return false;
  }
}

// Auto-tune a PI controller with kD = 0
// using SIMC tuning method
void PID::initPITuning(uint16_t steadyTreshold, float noiseTreshold) {

  // Set initial output 50% of maximum
  _tuningOutput1 = _limitMin / 2 + _limitMax / 2;

  // DEBUG
  debugPrint("Output 1: ", false);
  debugPrint(_tuningOutput1);

  // Set step output 30% of initial output
  _tuningOutput2 = 1.3f * _tuningOutput1;

  // DEBUG
  debugPrint("Output 2: ", false);
  debugPrint(_tuningOutput2);

  // Set tresholds
  _noiseTreshold = noiseTreshold;
  _steadyTreshold = steadyTreshold;

  // Tuning state reflects experiment stages
  _tuningState = 0;

  _outputStart = *_output;

  // Reset process dead time
  _theta1 = _theta2 = 0;

  // Reset process time constant
  _processTime = 0;

  // Reset input change, stabilized input before a step change
  _inputStart = *_input;
  _inputChange = 0;

  _steadyCount = 0;

  _startTuningTime = millis();
}

// Setup kalman filter for tuning
// http://interactive-matter.eu/blog/2009/12/18/filtering-sensor-data-with-a-kalman-filter/
void PID::initFilter(filter_t *filterState, boolean useFilter) {
  _filterState = filterState;
  _useFilter = useFilter;

  if (useFilter) {
    _useFilter = true;
  }
}

// Apply a Kalman filter
void PID::_doFilter() {
  if (_useFilter) {
    _filterState->p = _filterState->p + _filterState->q;
    _filterState->k = _filterState->p / (_filterState->p + _filterState->r);
    *_input = _previousInput + _filterState->k * (*_input - _previousInput);
    _filterState->p = (1 - _filterState->k) * _filterState->p;
  }
}

// Run the tuning iteration
boolean PID::doPITuning() {

  // Tuning step 1 - stabilize at initial output, get theta value
  if (_tuningState == 0) {
    if (_doTuningStep1()) {
      _tuningState = 1;
      _steadyCount = 0;
      _startTuningTime = millis();
      _inputStart = *_input;

      // DEBUG
      debugPrint("Finished step 1");
      debugPrint("Stabilized at ", false);
      debugPrint(*_input);

      // Tuning isn't completed yet
      return false;
    }
  }

  // Tuning step 2 - make a step to find out input change
  if (_tuningState == 1) {
    if (_doTuningStep2()) {
      _tuningState = 2;
      _steadyCount = 0;
      _startTuningTime = millis();
      _inputStart = *_input;

      // DEBUG
      debugPrint("Finished step 2");
      debugPrint("Stabilized at ", false);
      debugPrint(*_input);

      return false;
    }
  }

  // Tuning step 3 - step back to initial stabilized output
  if (_tuningState == 2) {
    if (_doTuningStep3()) {
      _tuningState = 3;
      _steadyCount = 0;
      _startTuningTime = millis();
      _inputStart = *_input;

      // DEBUG
      debugPrint("Finished step 3");
      debugPrint("Stabilized at ", false);
      debugPrint(*_input);

      return false;
    }
  }

  // Tuning step 4 - make step to find out time to reach 63% of input change
  if (_tuningState == 3) {
    if (_doTuningStep4()) {
       // DEBUG
      debugPrint("Finished step 4");
      debugPrint("Stabilized at ", false);
      debugPrint(*_input);

      _theta = (_theta1 + _theta2) / 2;

      // DEBUG
      debugPrint("Theta: ", false);
      debugPrint(_theta);
      debugPrint("Process time: ", false);
      debugPrint(_processTime);
      debugPrint("Input change: ", false);
      debugPrint(_inputChange);
      debugPrint("Output change: ", false);
      debugPrint(_tuningOutput2 - _tuningOutput1);

      float slope = _inputChange / ((_tuningOutput2 - _tuningOutput1) * _processTime);
      float tauC = _processTime > 8 * _theta ? _processTime : 8 * _theta;

      float kC = (1 / slope) * (1 / (_theta + tauC));
      //float tauI = _processTime < 4 * (tauC + _theta) ? _processTime : 4 * (tauC + _theta);
      float tauI = _processTime;

      setKs(kC, kC / tauI, 0);

      return true;
    }
  }

  _previousInput = *_input;
  return false;
}

unsigned long PID::getStableTime() {
  float stepTime = _processTime * 100.0f / 63.0f;
  return (long)stepTime;
}

boolean PID::_doTuningStep1() {

  float deltaPrevious = 0;
  *_output = _tuningOutput1;
  float deltaInput = *_input - _inputStart;
  deltaInput = abs(deltaInput);

  if ((deltaInput >= _noiseTreshold) && (_theta1 == 0) && _steadyCount > 3) {
      _theta1 = millis();
      _theta1 -= 3 * _timeStep;

      // DEBUG
      debugPrint("Theta 1 found: ", false);
      debugPrint(_theta1);
  }

  if (deltaInput >= _noiseTreshold) {
    deltaPrevious = *_input - _previousInput;
    // DEBUG
    debugPrint("Delta previous: ", false);
    debugPrint(deltaPrevious);

    if (abs(deltaPrevious) <= _noiseTreshold) {
      _steadyCount++;

      // DEBUG
      debugPrint("Stable for: ", false);
      debugPrint(_steadyCount);
    } else {
      _steadyCount = 0;
    }

    if (_steadyCount > _steadyTreshold) {
      _theta1 -= _startTuningTime;
      return true;
    }

  }

  return false;
}

boolean PID::_doTuningStep2() {
  float deltaPrevious = 0;
  *_output = _tuningOutput2;
  float deltaInput = *_input - _inputStart;
  deltaInput = abs(deltaInput);

  if ((deltaInput >= _noiseTreshold) && (_theta2 == 0) && _steadyCount > 3) {
      _theta2 = millis();
      _theta2 -= 3 * _timeStep;
  }

  if (deltaInput >= _noiseTreshold) {
    deltaPrevious = *_input - _previousInput;
    if (abs(deltaPrevious) <= _noiseTreshold) {
      _steadyCount++;
    } else {
      _steadyCount = 0;
    }

    if (_steadyCount > _steadyTreshold) {
      _theta2 -= _startTuningTime;
      _inputChange = *_input - _inputStart;
      _inputChange = abs(_inputChange);
      return true;
    }

  }

  return false;
}

boolean PID::_doTuningStep3() {
  *_output = 0;
  float deltaPrevious = _direction ? _inputStart - _inputChange : _inputStart + _inputChange;
  float deltaInput = *_input - deltaPrevious;
  deltaInput = abs(deltaInput);

  if (deltaInput <= _noiseTreshold) {
    return true;
  }

  return false;
}

boolean PID::_doTuningStep4() {

  float inputTreshold = 63.0f * _inputChange / 100.0f;
  *_output = _tuningOutput2;
  float deltaInput = *_input - _inputStart;
  deltaInput = abs(deltaInput);

  if (deltaInput >= inputTreshold) {
    _processTime = timeDiff(_startTuningTime);
    return true;
  }

  return false;
}
