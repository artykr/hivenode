# Arduino-based home automation system prototype

This is a little bit old set of modules for building an network of Arduino "nodes" with sensors and actuators connected. Each node supposed to be an **Arduino Mega 2560** because of memory requirements and can be configured to support any number of sensor/actuators limited by memory size only. A node communicates over HTTP using RESTful API. It's not too fast but that's a trade off for using JSON. It means, for example, you can connect a sensor like a DS1820 and get its readings as a JSON payload through HTTP. You can connect a lightbulb and a switch and drive it through JSON/HTTP.
I tried hard to keep the memory footprint small and avoid memory leaks, but still a JSON parsing library is a hard piece for an Arduino.

External libraries used in modules has been put into `/libraries subfolder`. These are old versions but you can find corresponding GitHub sources by looking into source files.

Each file has comments inside. I haven't tested it with a recent version of Arduino IDE and libraries and I'm not really sure every sensor/actuator module is perfectly debugged and has 100% correct logic implemented, but I hope some of the code could be useful.

To get a working node describe connected sensors and actuators in `HiveSetup.h / HiveSetup.cpp`, load `hive.ino` sketch in Arduino IDE, compile and upload to your board.

## Files description

- `AppContext`: a class for a context object which holds application-wide information and is usually accessible in any class and method.
- `DeviceDispatch`: a helper class for selecting an SPI device (e.g. SD card shield or an ethernet shield).
- `DHTSensor`: a DHT sensor class. If a DHT sensor is connected to the board it should be initialized in `HiveSetup.cpp`.
- `DHTSwitch`: a class to drive a humidity-based switch. Switches on when humidity value has crossed some threshold and keeps working for a predefined period of time.
- `FallbackSwitch`: actually a usual light switch with manual on/off override mode but with a fallback relay. The fallback relay is normally closed and makes the circuit drive the light by the switch like there's no Arduino connected to it. The board toggles this relay at initialization and takes control over the switch. If something happens to the board so it is not initialized the switch falls back to a simple "non-smart" mode. It actually makes the circuit more complex but safer for a user.
- `FloorHeater`: a module to drive an electric floor heating circuit. It requires OWTSensor (One-Wire-Temperature Sensor) module to be initialized first. It uses the PID module for tuning and control and has a configurable schedule (three periods for each day of week with different temperatures).
- `HiveSetup`: configuration file for a node. Put all sensors/actuators initialization values here.
- `HiveStorage`: a class for storing settings. Settings can be stored using either in EEPROM or an SD card (can be defined it in `HiveSetup`).
- `HiveUtils`: utilities for the debug output and time calculations.
- `LightSwitch`: simple light switch module. Same as `FallbackSwitch` but without a fallback relay.
- `OWTSensor`: a DS1820 (and alike) temperature sensor class.
- `PID`: a PID implementation with [SIMC](http://www.nt.ntnu.no/users/skoge/publications/2012/skogestad-improved-simc-pid/old-submitted/simcpid.pdf) auto-tuning method. This module has to be tested more thoroughly.
- `PirSwitch`: a module for driving a PIR sensor and a relay circuit. Could be useful for an auto on/off light.
- `SensorModule`: a base class for sensor/actuator modules.
- `WebStream`: a Stream wrapper for Webduino library.
