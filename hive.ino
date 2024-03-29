// Don't waste SRAM for a default Webduino favicon
#define WEBDUINO_FAVICON_DATA ""

// Override Webduino default fail message
#define WEBDUINO_FAIL_MESSAGE "<h1>Request Failed</h1>"

// Arduino includes and standard libraries
#include "Arduino.h"
#include "EEPROM.h"
#include "Wire.h"
#include "SPI.h"
#include "SD.h"
#include "Ethernet.h"

// External libraries
#include "WebServer.h"
#include "OneWire.h"
#include "DallasTemperature.h"
#include "aJSON.h"
#include "ds3231.h"
#include "DHT.h"

// Hive libraries
#include "HiveSetup.h"
#include "SensorModule.h"
#include "DeviceDispatch.h"
#include "HiveUtils.h"
#include "HiveStorage.h"
#include "WebStream.h"
#include "MemoryFree.h"

char requestBuffer[RestRequestLength];
int requestLen = RestRequestLength;

// Store remote IP for push notifications
IPAddress clientIPAddress(0, 0, 0, 0);

const uint8_t CommonBufferLength = 32;

// Store remote domain name for push notifications
char clientDomain[CommonBufferLength];

// Store remote URL for push notifications
char clientURL[CommonBufferLength];

// Store remote port
int16_t clientPort = 80;

// Create a WebServer instance
WebServer nodeWebServer("", 80);

// If Ethernet is initialized and nodeWebServer is started - set it to TRUE
boolean webServerActive = false;

aJsonObject *moduleCollection;
aJsonObject *moduleItem;

AppContext context(&moduleCollection, &pushNotify);

boolean pushNotify(byte moduleId) {
  // We'll set it to true if a server has discovered our node
  boolean isDiscovered = false;

  // DEBUG
  char host[CommonBufferLength];

  char buffer[CommonBufferLength];

  // Select SPI slave device - Ethernet
  useDevice(DeviceIdEthernet);

  // Create an object for making HTTP requests
  EthernetClient client;

  if (webServerActive) {
    // If we have a domain, connect with it
    if ((clientDomain != NULL) && (strlen(clientDomain) > 0)) {
      isDiscovered = true;
      client.connect(clientDomain, clientPort);
      strncpy(host, clientDomain, sizeof(host) - 1);
      // Null-terminate the string just in case
      if (sizeof(host) > 0)
      {
        host[sizeof(host)-1] = 0;
      }
    } else {
      // If we have ip address only connect with it
      if(clientIPAddress[0] > 0) {

        isDiscovered = true;

        // DEBUG
        for (int i = 0; i < 4; i++) {
          itoa(clientIPAddress[i], buffer, 10);
          strncat(host, buffer, strlen(buffer));
          if (i < 3)
            strncat(host, ".", 1);
        }
        debugPrint(F("Push IP found: "), false);
        debugPrint(host);

        client.connect(clientIPAddress, clientPort);
      }
    }

    if (isDiscovered && client.connected()) {
      //DEBUG
      debugPrint(F("Connected to server"));

      // Call remote URL with moduleId attached
      // TODO: use no-cost stream operator for printing to client

      // Send "GET clientURL/nodeId/moduleId HTTP/1.0"
      // so the remote site knows which pcb/module settings changed

      client.print(F("GET "));
      client.print(clientURL);
      client.print(F("/"));
      client.print(nodeId);
      client.print(F("/"));
      client.print(moduleId);
      client.println(F(" HTTP/1.0"));
      client.println();

      if (client.connected()) {
        if (client.find("success")) {
          // DEBUG
          debugPrint("Success");

          client.stop();
          return true;

        }
      }
    }

    client.stop();
  }

  return false;
}

// Process a REST request for an item (module) (URL contains ID)
void webItemRequest(WebServer &server, WebServer::ConnectionType type, long *moduleId) {

  // Define moduleCollelction array item index which is moduleId - 1
  int i = *moduleId - 1;

  // DEBUG
  debugPrint(F("Processing item request: "), false);
  debugPrint(*moduleId);

  // Module id is out of range
  if ((*moduleId > modulesCount) || (*moduleId < 1)) {
    // TODO: output an error object with failure headers
    server.httpFail();
    return;
  }

  // aJson uses a wrapper for streams
  WebStream webStream(&server);
  aJsonStream jsonStream(&webStream);

  switch (type) {
    case WebServer::GET:

      // Process request for a single module (item) settings

      server.httpSuccess("application/json");
      sensorModuleArray[i]->getJSONSettings();
      moduleItem = aJson.getArrayItem(moduleCollection, i);
      aJson.print(moduleItem, &jsonStream);

      break;
    case WebServer::PUT:
    {
      // Process request for settings change

      // Parse request to JSON
      // No aJson filter used here for simplicity, speed and memory usage
      // TODO: get aJson filter from module and apply it here

      aJsonObject *newModuleItem = aJson.parse(&jsonStream);

      // DEBUG
      debugPrint(F("Received data: "), false);
      char *json1 = aJson.print(newModuleItem);
      debugPrint(json1);
      free(json1);

      // Set the parsed settings
      if (sensorModuleArray[i]->setJSONSettings(newModuleItem)) {

        server.httpSuccess("application/json");

        // Refresh module settings in JSON collection
        sensorModuleArray[i]->getJSONSettings();
        moduleItem = aJson.getArrayItem(moduleCollection, i);

        // Print settings back to the client
        aJson.print(moduleItem, &jsonStream);

      } else {
        server.httpFail();
      }

      aJson.deleteItem(newModuleItem);

      break;
    }
    default:
      server.httpFail();
  }
}

// Process request for the whole items (modules) settings collection
void webCollectionRequest(WebServer &server, WebServer::ConnectionType type) {

  WebStream webStream(&server);
  aJsonStream jsonStream(&webStream);

  switch (type) {
    case WebServer::GET:
    {
      server.httpSuccess("application/json");
      // Reload JSON structures for each module
      for (int i = 0; i < modulesCount; i++) {
        sensorModuleArray[i]->getJSONSettings();
      }

      // Print out the whole collection
      aJson.print(moduleCollection, &jsonStream);

      break;
    }
    default:
      server.httpFail();
  }
}

// Routine is called by the web server when ANY request arrives.
// Process parts of the whole url in url_path array,
// check if it's a REST request and process it
void dispatchRESTRequest(WebServer &server, WebServer::ConnectionType type,
                        char **url_path, char *url_tail,
                        bool tail_complete) {
  long moduleId = 0;

  // For a HEAD request return only headers
  if (type == WebServer::HEAD) {
    server.httpSuccess();
    return;
  }

  // DEBUG
  debugPrint(F("Processing REST request..."));

  // RESTful interface structure:
  // /modules
  //      GET - outputs json structure for all modules
  //      PUT - updates settings for all modules in a batch (not supported: too large for request buffer)
  // /modules/<id>
  //      GET - outputs json structure for a module with moduleId == <id>
  //      PUT - updates settings for a module with moduleId == <id>

  if (strcmp(url_path[0], "modules") == 0) {

    // DEBUG
    debugPrint(F("Found /modules in request URL"));

    if (url_path[1]) {
      // We use safe strtol instead of unsafe atoi at a cost
      // of moduleId being of type long
      moduleId = strtol(url_path[1], NULL, 10);
    }

    if (moduleId > 0) {

      // We deal with a single module request
      webItemRequest(server, type, &moduleId);
      return;

    } else {

      // We deal with the whole modules collection request
      webCollectionRequest(server, type);
      return;

    }

  }

  server.httpFail();

}

// Handle discovery mode, record remote server url for push notifications,
// respond with node information
void webDiscoverCommand(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete) {
  aJsonObject *clientInfo;
  aJsonObject *infoItem;
  WebStream webStream(&server);
  aJsonStream jsonStream(&webStream);

  byte i = 0;

  // For a HEAD request return only headers
  if (type == WebServer::HEAD) {
    server.httpSuccess();
    return;
  }

  if (type == WebServer::POST) {

    // We could use filtering, but it's memory consuming
    // and may be omitted in merely safe environements

    //char **jsonFilter = (char *[]){"domain", "url", "ip", NULL};
    //clientInfo = aJson.parse(&jsonStream, jsonFilter);

    clientInfo = aJson.parse(&jsonStream);

    // DEBUG
    debugPrint(F("Domain and url from server: "), false);
    char *json = aJson.print(clientInfo);
    debugPrint(json);
    free(json);

    // Discover request structure
    // JSON object with fields:
    // domain - string, for POST request easy building (32 chars max)
    // ip - object, if we have no local DNS and need to talk through ip address
    //    o1, o2, o3, o4 - numbers, ip address octets so we don't need to parse the address
    // url - string, URL tail (without domain), begins with a slash (32 chars max)

    // Get remote domain from json
    infoItem = aJson.getObjectItem(clientInfo, "domain");

    if (infoItem && (infoItem->type == aJson_String)) {
      strncpy(clientDomain, infoItem->valuestring, sizeof(clientDomain) - 1);

      // Null-terminate the string just in case
      if (sizeof(clientDomain) > 0)
      {
        clientDomain[sizeof(clientDomain)-1] = 0;
      }
    }

    // Get remote url
    infoItem = aJson.getObjectItem(clientInfo, "url");
    if (infoItem && (infoItem->type == aJson_String)) {
      strncpy(clientURL, infoItem->valuestring, sizeof(clientURL) - 1);
      // Null-terminate the string just in case
      if (sizeof(clientURL) > 0)
      {
        clientURL[sizeof(clientURL)-1] = 0;
      }
    }

    infoItem = aJson.getObjectItem(clientInfo, "ip");

    if (infoItem) {
      infoItem = infoItem->child;

      while (infoItem && (i < 4)) {
        if (infoItem->type == aJson_Int) {
          clientIPAddress[i] = infoItem->valueint;
        }
        infoItem = infoItem->next;
        i++;
      }
    }

    infoItem = aJson.getObjectItem(clientInfo, "port");

    if (infoItem && (infoItem->type == aJson_Int)) {
      clientPort = infoItem->valueint;
    }

    // TODO: validate the whole structure
    server.httpSuccess("application/json");

    infoItem = aJson.createObject();
    // Prepare JSON structure containing pcb id and number of sensor modules
    aJson.addItemToObject(infoItem, "id", aJson.createItem(nodeId));
    aJson.addItemToObject(infoItem, "modulesCount", aJson.createItem(modulesCount));
    // Output response and delete aJson object
    aJson.print(infoItem, &jsonStream);

    aJson.deleteItem(infoItem);
    aJson.deleteItem(clientInfo);

    return;
  }

  server.httpFail();
}

// Handle system status information request
void webInfoCommand(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete) {
  WebStream webStream(&server);
  aJsonStream jsonStream(&webStream);
  aJsonObject *infoItem;

  // DEBUG
  debugPrint(F("Processing info request..."));

  switch (type) {
    case WebServer::HEAD:
    {
      server.httpSuccess();
      break;
    }
    case WebServer::GET:
    {
      server.httpSuccess("application/json");
      // Reload JSON structures for each module

      infoItem = aJson.createObject();
      aJson.addItemToObject(infoItem, "memory", aJson.createItem(freeMemory()));
      aJson.addItemToObject(infoItem, "storage", aJson.createItem(StorageType));

      // Print out the info object
      aJson.print(infoItem, &jsonStream);
      aJson.deleteItem(infoItem);

      break;
    }
    default:
      server.httpFail();
  }
}

// Generate MAC address and store it in available storage
void setupMACAddress(boolean loadSettings) {

  if (!loadSettings) {
    randomSeed(analogRead(0));
    for (int i = 1; i < 4; i++) {
      hivemac[i+2] = random(0, 255);
    }

    saveSystemSettings();
  }

}

boolean setupServer() {
  boolean ethernetEnabled = false;

  // DEBUG
  debugPrint(F("Starting Ethernet..."));

  // Select Ethernet on SPI
  useDevice(DeviceIdEthernet);

  // If we have a static IP
#ifdef HIVE_STATIC_IP
  Ethernet.begin(hivemac, nodeIPAddress);
  ethernetEnabled = true;
#endif

#ifndef HIVE_STATIC_IP
  if (Ethernet.begin(hivemac)) {
    ethernetEnabled = true;
  }
#endif

  if (ethernetEnabled) {
    // Give Ethernet time to initialize
    delay(1000);

    // DEBUG
    debugPrint(F("Ethernet started"));

#ifndef HIVE_STATIC_IP
    // Only get ID if received through DHCP
    nodeIPAddress = Ethernet.localIP();
#endif

    // DEBUG
    debugPrint(F("IP address: "), false);
    Serial.println(nodeIPAddress);

    nodeWebServer.addCommand("discover", &webDiscoverCommand);
    nodeWebServer.addCommand("info", &webInfoCommand);
    nodeWebServer.setUrlPathCommand(&dispatchRESTRequest);
    //nodeWebServer.setFailureCommand(&webFailureCommand);
    nodeWebServer.begin();

    return true;
  }

  // DEBUG
  debugPrint(F("Failed to start Ethernet"));

  return false;
}

void addToJSONCollection(byte id) {
  //aJsonObject *moduleItem;
  moduleItem = aJson.createObject();
  aJson.addItemToObject(moduleItem, "id", aJson.createItem(id));
  aJson.addItemToArray(moduleCollection, moduleItem);

}

void setup() {
  boolean systemSettingsLoaded = false;
  boolean moduleSettingsExist = false;

  systemSettingsLoaded = loadSystemSettings();

  // Init SPI CS pins
  initPins();

#ifdef HIVE_DEBUG
  Serial.begin(9600);
  delay(1000);
#endif

  // DEBUG
  debugPrint(F("Free memory on start: "), false);
  debugPrint(freeMemory());

  useDevice(DeviceIdEthernet);
  // Init Ethernet first so it responds to
  // SPI slave select command
  setupMACAddress(systemSettingsLoaded);
  webServerActive = setupServer();

  useDevice(DeviceIdSD);
  // Init storage and check if there are some
  // modules settings stored
  moduleSettingsExist = initStorage();

  // Define and init modules
  initModules(&context, moduleSettingsExist);

  // Create and fill json structure for the REST interface
  moduleCollection = aJson.createArray();

  // TODO: Start cycle through SensorModuleArray
  for (byte i = 1; i <= modulesCount; i++) {
    // Create an empty item in json collection
    addToJSONCollection(i);

    // Add modules settings to this item through the context object
    sensorModuleArray[i - 1]->getJSONSettings();
  }

  // DEBUG
  debugPrint(F("Context collection: "), false);
  char *json = aJson.print(*context.moduleCollection);
  debugPrint(json);
  free(json);

}

void loop() {
  // Call each module loop method
  for(byte i = 0; i < modulesCount; i++) {
    sensorModuleArray[i]->loopDo();
  }

  // Check for web server calls
  if (webServerActive) {
    useDevice(DeviceIdEthernet);
    nodeWebServer.processConnection(requestBuffer, &requestLen);
  }
}

void handleInterrupts(uint8_t handlerIndex) {
  /*if (interruptModuleArray[handlerIndex]) {
    interruptModuleArray[handlerIndex]->handleInterrupt();
  }*/
}

// TODO: check for the AVR variant here
ISR(TIMER3_OVF_vect) {
  handleInterrupts(0);
}

ISR(TIMER4_OVF_vect) {
  handleInterrupts(1);
}

ISR(TIMER5_OVF_vect) {
  handleInterrupts(2);
}
