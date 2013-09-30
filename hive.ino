#include "avr/pgmspace.h"
#include "SensorModule.h"
#include "LightSwitch.h"
#include "aJSON.h"
#include "SPI.h"
#include "Ethernet.h"

// Turn on WebServer debugging
#define WEBDUINO_SERIAL_DEBUGGING 0

#include "WebServer.h"
#include "EEPROM.h"
#include "EEPROMAnything.h"
#include "WebStream.h"
#include "AppContext.h"
#include "MemoryFree.h"


// DEBUG variables for getting free memory size
extern int __bss_end;
extern void *__brkval;

#define REST_REQUEST_LENGTH 255

char requestBuffer[REST_REQUEST_LENGTH];
int requestLen = REST_REQUEST_LENGTH;

// This PCB id
byte nodeId = 1;

// Define zones and count
const byte hallZone = 1;
const byte kitchenZone = 2;
const byte zoneCount = 2;
const char *zones[] = {"", "hallZone", "kitchenZone"};

// Total number of modules on board
const byte modulesCount = 1;

// Storage offset - modules settings come after the offset.
// This is the size of general settings (if any)
const byte settingsOffset = 4;

// ****
// INPORTANT: remember to connect SS pin from Ethernet module to pin 10 on Arduino Mega
// ****
// Define MAC adress of the Ethernet shield
// First 3 octets are OUI (Organizationally Unique Identifier)
// of GHEO Sa company. Other 3 octets will be randomly generated.
// orig. from http://nicegear.co.nz/blog/autogenerated-random-persistent-mac-address-for-arduino-ethernet/
byte mac[6] = { 0x90, 0xA2, 0xDA, 0x00, 0x00, 0x00 };

// Store IP from DHCP request
IPAddress nodeIPAddress;

// Store remote IP
IPAddress clientIPAddress(0, 0, 0, 0);

// Store remote domain name
char *clientDomain;

// Store remote URL
char *clientURL;

// Create a WebServer instance
WebServer nodeWebServer("", 80);

// Create an object for making HTTP requests
EthernetClient client;

// If Ethernet is initialized and nodeWebServer is started - set to TRUE
boolean webServerActive = false;

// Define structure to hold all sensor modules

SensorModule *sensorModuleArray[modulesCount];
aJsonObject *moduleCollection;
aJsonObject *moduleItem;
AppContext context(&moduleCollection, &pushNotify);                     

boolean pushNotify(byte moduleId) {
  // TODO: remove postJSON, so push notifications are just GET requests with module id - remote client will make a callback with this id
/*  char *host;
  char buffer[32];
  
  aJsonObject *response;
  
  if (webServerActive) {
    // If we have a domain, connect with it
    if ((clientDomain != NULL) && (strlen(clientDomain) > 0)) {
      client.connect(clientDomain, 80);
      host = clientDomain;
    } else {
      // If we have ip address only connect with it
      if(clientIPAddress[0] > 0) {
        client.connect(clientIPAddress, 80);
        sprintf(host, "%d.%d.%d.%d",
          clientIPAddress[0],
          clientIPAddress[1],
          clientIPAddress[2],
          clientIPAddress[3]
        );
      } 
    }
    
    if (client.connected()) {
      // Call remote URL with moduleId attached
      sprintf(buffer, "GET %s/%d HTTP/1.0", clientURL, moduleId);
      client.println(buffer);
      sprintf(buffer, "Host: %s", host);
      client.println(buffer);
      client.println("Connection: close\r\n");
      
      if (client.connected() && client.available()) {
        // Read JSON response from remote

        char **jsonFilter = (char *[]){ "response", NULL };
        aJsonClientStream jsonStream(&client);
        response = aJson.parse(&jsonStream, jsonFilter);
        // Response structure:
        // "response" : "success"
        // "response" : "error"
        
        response = aJson.getObjectItem(response, "response");
        
        if (strcmp(response->valuestring, "success") == 0) {
          aJson.deleteItem(response);
          return true;
        }
      }
    }
  }
  
  aJson.deleteItem(response);
  return false;*/
}

// Process a REST request for an item (module) (URL contains ID)
void webItemRequest(WebServer &server, WebServer::ConnectionType type, int *moduleId) {
  
  // Define array index which is moduleId - 1
  int i = *moduleId - 1;

  // DEBUG
  Serial.print(F("Processing item request: "));
  Serial.println(*moduleId);

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
      
        // DEBUG
      Serial.print(F("Free memory after item call: "));
      Serial.println(DEBUG_memoryFree());

      break;
    case WebServer::PUT:
    {
      // Process request for settings change

      // Parse request to JSON
      // No aJson filter used here for simplicity, speed and memory usage
      // TODO: get aJson filter from module and apply it here

      moduleItem = aJson.parse(&jsonStream);

      // Set the parsed settings

      if (sensorModuleArray[i]->setJSONSettings(moduleItem)) {

        server.httpSuccess("application/json");

        // Refresh module settings in JSON collection
        sensorModuleArray[i]->getJSONSettings();
        moduleItem = aJson.getArrayItem(moduleCollection, i);

        // Print settings back to the client
        aJson.print(moduleItem, &jsonStream);

      } else {
        server.httpFail();
      }
      break;
    }
    default:
      server.httpFail();
  }
}

// Process request for the whole items (modules) settings collection 
void webCollectionRequest(WebServer &server, WebServer::ConnectionType type) {
  aJsonObject *newModuleCollection;
  boolean errorInRequest = false;
  byte moduleId;
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

        aJson.print(moduleCollection, &jsonStream);

        break;
      }
    /*

    // Bulk settings update is memory consumig
    // We have only 255 chars buffer for a request by default (REST_REQUEST_LENGTH)

    case WebServer::PUT:
      {
        newModuleCollection = aJson.parse(&jsonStream);
        // TODO: replace only valid items in collection
        moduleItem = newModuleCollection->child;
        
        while (moduleItem) {
          moduleId = aJson.getObjectItem(moduleItem, "id")->valueint;
          if (moduleId > modulesCount || moduleId < 1) {
            // Set error flag if settings are invalid
            errorInRequest = !sensorModuleArray[moduleId-1]->setJSONSettings(moduleItem);
          } else {
            errorInRequest = true;
          }
          moduleItem = moduleItem->next;
        }
        
        // TODO: return error object with failed module ids
        if (errorInRequest) {
          server.httpFail();
        } else {
          server.httpSuccess("application/json");
          aJson.print(moduleCollection, &jsonStream);
        }
        break;
      }
      */
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
  int moduleId = 0;

  // For a HEAD request return only headers
  if (type == WebServer::HEAD) {
    server.httpSuccess();
    return;
  }
  // RESTful interface structure:
  // /modules
  //      GET - outputs json structure for all modules
  //      PUT - updates settings for all modules in a batch (not supported: too large for request buffer)
  // /modules/<id>
  //      GET - outputs json structure for a module with moduleId == <id>
  //      PUT - updates settings for a module with moduleId == <id>
  
  // TODO: check if the 0 element in url_path really contains something
  if (strcmp(url_path[0], "modules") == 0) {
    if (url_path[1]) {
      moduleId = atoi(url_path[1]);
    }
    
    if (moduleId) {

      // We deal with a single module
      webItemRequest(server, type, &moduleId);
      return;
    } else {

      // We deal with the whole modules collection
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
    char **jsonFilter = (char *[]){"domain", "url", "ip", NULL};
    clientInfo = aJson.parse(&jsonStream, jsonFilter);
    
    // Discover request structure:
    // JSON object with fields
    // domain - string, for POST request easy building
    // ip address - object, if we have no local DNS and need to talk through ip address
    //    o1, o2, o3, o4 - numbers, ip address octets so we don't need to parse the address
    // url - string, URL tail (without domain), begins with a slash
    
    // Get remote domain from json
    infoItem = aJson.getObjectItem(clientInfo, "domain");
    if (infoItem && (infoItem->type == aJson_String)) {
      clientDomain = infoItem->valuestring;
    }
    
    // Get remote url
    infoItem = aJson.getObjectItem(clientInfo, "url");
    if (infoItem && (infoItem->type == aJson_String)) {
      clientURL = infoItem->valuestring;
    }
    
    infoItem = aJson.getObjectItem(clientInfo, "ip");
    if (infoItem) {
      infoItem = infoItem->child;
    
      while (infoItem && (i < 3)) {
        if (infoItem->type == aJson_Int) {
          clientIPAddress[i] = infoItem->valueint;
        }
        infoItem = infoItem->next;
        i++;
      }

      aJson.deleteItem(clientInfo);
      aJson.deleteItem(infoItem);
    }
    
    // TODO: validate the whole structure
    server.httpSuccess("application/json");
    // Reply with nodeid, zones, modules count
    clientInfo = aJson.createObject();
    aJson.addItemToObject(clientInfo, "nodeId", aJson.createItem(nodeId));
    aJson.addItemToObject(clientInfo, "modulesCount", aJson.createItem(modulesCount));
    aJson.addItemToObject(clientInfo, "zones", infoItem = aJson.createObject());
    for (int i = 1; i < zoneCount; i++) {
      aJson.addItemToObject(infoItem, zones[i], aJson.createItem(i));
    }

    aJson.print(clientInfo, &jsonStream);
    aJson.deleteItem(clientInfo);
    aJson.deleteItem(infoItem);
  }
  
  server.httpFail();
}

// General failure command
// TODO: Check whether custom implementation is really needed
void webFailureCommand(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete) {
  server.httpFail();
  return;
}

boolean setupServer() {
  // DEBUG
  Serial.println(F("Starting Ethernet..."));

  if (Ethernet.begin(mac)) {
    // Give Ethernet time to initialize
    delay(2000);

    // DEBUG
    Serial.println(F("Ethernet started"));

    nodeIPAddress = Ethernet.localIP();
    
    // DEBUG
    Serial.print(F("IP address: "));
    Serial.println(nodeIPAddress);

    nodeWebServer.setUrlPathCommand(&dispatchRESTRequest);
    nodeWebServer.addCommand("discover", &webDiscoverCommand);
    nodeWebServer.setFailureCommand(&webFailureCommand);
    nodeWebServer.begin();
    
    return true;
  }
  
  // DEBUG
  Serial.println(F("Failed to start Ethernet"));

  return false;
}


void addToJSONCollection(byte id) {

  moduleItem = aJson.createObject();
  aJson.addItemToObject(moduleItem, "id", aJson.createItem(id));
  aJson.addItemToArray(moduleCollection, moduleItem);

}

// Generate MAC address and store it in EEPROM
void setupMACAddress(boolean loadSettings) {

  // TODO: Separate loading and saving from MAC generation

  // Load stored settings
  if (loadSettings) {
    for (int i = 1; i < 4; i++) {
      mac[i+2] = EEPROM.read(i);
    }
  // If there are no stored settings, generate 3 octets of MAC address
  } else {
    randomSeed(analogRead(0));
    for (int i = 1; i < 4; i++) {
      mac[i+2] = random(0, 255);
      EEPROM.write(i, mac[i+2]);
    }
  }

}

// Arduino setup routine
void setup() {
  int lastStoragePointer = settingsOffset;
  byte storageMark = 0;
  byte moduleId = 1;
  boolean loadSettings = 0;
  
  // DEBUG
  Serial.begin(9600);
  delay(2000);

  Serial.print(F("Free memory on start: "));
  Serial.println(DEBUG_memoryFree());

  // Load and check settings flag byte, set settings load flag for modules
  
  EEPROM_readAnything(0, storageMark);
  
  // DEBUG
  Serial.print(F("Storage mark: "));
  Serial.println(storageMark);

  storageMark == 99 ? loadSettings = 1 : loadSettings = 0;
 
  // Create and fill json structure for the REST interface
  moduleCollection = aJson.createArray();
  
  // Create objects for all sensor modules and init each one
  sensorModuleArray[0] = new LightSwitch(&context, hallZone, moduleId, lastStoragePointer, loadSettings, 8, 4);
  
  // Create an empty item in json collection and add modules settings to this item through the context object
  addToJSONCollection(moduleId);

  // DEBUG
  Serial.print(F("Context collection: "));
  char *json = aJson.print(*context.moduleCollection);
  Serial.println(json);
  free(json);

  sensorModuleArray[0]->getJSONSettings();
  
  // DEBUG
  Serial.print(F("Current collection: "));
  char *json1 = aJson.print(moduleCollection);
  Serial.println(json1);
  free(json1);

  moduleId++;
  lastStoragePointer += sensorModuleArray[0]->getStorageSize();

  setupMACAddress(loadSettings);
  webServerActive = setupServer();

  // DEBUG
  if (webServerActive) {
    Serial.println(F("Web Server Started"));
  }

  // Set storage mark after all modules init
  if (!loadSettings) EEPROM_writeAnything(0, 99);
  
  // DEBUG
  Serial.print(F("Free memory after setup: "));
  Serial.println(DEBUG_memoryFree());

  delay(1000);
}

void loop() {

  // Call each module loop method
  for(byte i = 0; i < modulesCount; i++) {
    sensorModuleArray[i]->loopDo();
  }
  
  // Check for web server calls
  if (webServerActive) {
    nodeWebServer.processConnection(requestBuffer, &requestLen);
  }
  
}

int DEBUG_memoryFree() {
  return freeMemory();
}

