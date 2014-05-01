#include "Arduino.h"
#ifndef PID_h
#define PID_h
#define PID_MODULE_VERSION 1

class PID
{
  typedef struct filter_t // Structure for Kalman filter state
  {
    float k;
    float p;
    float q;
    float r;
  };

  public:
    PID (
      float kP,
      float kI,
      float kD,
      float *input,
      float *output,
      float limitMin,
      float limitMax,
      float *setpoint,
      uint16_t timeStep,
      boolean direction
    );
    
    void setKs(float kP, float kI, float kD); // Sets PID coefs on the go
    float getKp();
    float getKi();
    float getKd();
    unsigned long getStepTime();
    void setTimeStep(uint16_t timeStep); // Sets sample time at which PID calculation is done
    void setOutputLimits(float limitMin, float limitMax); // Limits PID output
    void resetCalc();
    void setControlDirection(boolean direction);  // Changes PID direction, e.g.:
                                                  // bigger input - bigger output
                                                  // bigger input - smaller output
    boolean doControl();  // Calculates PID output
    void initPITuning(uint16_t steadyTreshold, float noiseTreshold);  // Init SIMC PID tuning
    void initFilter(filter_t *filterState, boolean useFilter); // Init Kalman filter
    boolean doPITuning(); // Runs PID tuning process

  private:

    float *_input;                    // Manipulated process output (controller input)
    float *_output;                   // Controller output (process input)
    float _limitMax;                  // Maximum controller output
    float _limitMin;                  // Minimum controller output
    float *_setpoint;                 // Desired process output
    unsigned long _timeStep;          // Controller sample time
    float _kP;                        // PID Kp (Kc) - controller gain
    float _kI;                        // PID Ki (Kc/Tc) - intergral term coef.
    float _kD;                        // PID Kd - derivative term coef.
    unsigned long _previousCalcTime;  // Helper for the loop time tracking
    float _error;                     // Controller error (input delta)
    float _previousInput;             // Helper
    float _integralTerm;              // PID integral term
    float _outputStart;               // Output value at tuning start
    float _inputStart;                // Input value at tuning start
    float _inputChange;               // Input change for a tuning step
    float _tuningOutput1;             // Tuning helper (lower step bound)
    float _tuningOutput2;             // Tuning helper (higher step bound)
    boolean _direction;               // Controller direction (true - direct, false - reverse)
    // Tuning variables
    uint8_t _tuningState;             // Wheter tuning is finished or in process
    unsigned long _startTuningTime;   // When the tuning process started
    unsigned long _processTime;       // Process time constant (Tp)
    unsigned long _theta1, _theta2;   // Process delays
    float _theta;                     // Process delay (mean of the above)
    boolean _useFilter;               // If process output (controller input) should be filtered
    filter_t *_filterState;           // Pointer to the filter state structure
    uint16_t _steadyCount;            // How long (in samples) the controller input is stable
    uint16_t _steadyTreshold;         // How long (in samples) it takes to mark input as stable
    float _noiseTreshold;             // Process noise level (noise band)

    void _constrainOutput();
    void _adjustIntegralTerm();
    void _doFilter();
    boolean _doTuningStep1();
    boolean _doTuningStep2();
    boolean _doTuningStep3();
    boolean _doTuningStep4();
};

#endif
