#include "DeviceDispatch.h"
#include "Arduino.h"

void useDevice(uint8_t deviceId) {
  uint8_t deviceCount = sizeof(DevicesCSPins) / sizeof(*DevicesCSPins);
  int8_t onIndex = -1;
  
  for (int i = 0; i < deviceCount; i++) {
    // Select the device we need
    if (i == deviceId) {
      onIndex = i;
    } else {
      // First disable all devices we don't need
      digitalWrite(DevicesCSPins[i], HIGH);
    }
  }
  
  if (onIndex >= 0) {
    // then enable device we need
    digitalWrite(DevicesCSPins[onIndex], LOW);
  }

}

void initPins() {
  uint8_t deviceCount = sizeof(DevicesCSPins) / sizeof(*DevicesCSPins);
  
  // DEBUG
  Serial.print("Device count: ");
  Serial.println(deviceCount);
  
  for (int i = 0; i < deviceCount; i++) {
    // Set all CS pins to output mode so the slave selection works
    pinMode(DevicesCSPins[i], OUTPUT);
  }
}