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

  for (int i = 0; i < deviceCount; i++) {
    // Set all CS pins to output mode so the slave selection works
    pinMode(DevicesCSPins[i], OUTPUT);
  }

// If we use a W5200 chip
#ifdef ETH_W5200
  pinMode(nRST, OUTPUT);
  pinMode(nPWDN, OUTPUT);
  pinMode(nINT, INPUT);

  digitalWrite(nPWDN,LOW);  //enable power

  digitalWrite(nRST, LOW);  //Reset W5200
  delay(10);
  digitalWrite(nRST, HIGH);
  delay(200);               // wait W5200 work
#endif

}
