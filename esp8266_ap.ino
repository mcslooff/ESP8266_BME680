/**
 * The GNU License
 * Copyright (c) 2019 by Marcel Slooff
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * =============================================================================
 * Writen and designed by Marcel Slooff. Based on the many examples and ideas
 * already available for the ESP8266 NodeMCU and Bosch BME680 modules.
 * 
 * This code aims, next to making the BME680 usefull in a home environment, to
 * provide an easy way of configuring the modules together. The advantage of
 * easy configuration is easy to see when you would like to deploy many of such
 * modules throughout your house (or would like to sell them).
 * 
 * This code provides:
 * - The module starting up in Access Point mode on IP 192.168.4.1
 * - Access Point accessible as ESP8266 with password ESP8266Test
 * - Web page for configuration accessible at the root with username admin
 *   and password admin.
 * - AP name, AP password, password and username can be changed via the 
 *   configuration page
 * - Support NTP synchronisation for timestamoing the measurements
 * - support two modes: pull and push for measurement publication.
 * - configurable measurement interval
 * - Change radio channel
 * 
 * Planned features:
 * - TLS support
 * - security on push messages for measurement publication
 * - dowmnload and upload configuration
 * - add end-point for submitting configuration changes with JSON
 * - Add batery monitoring on the ADC port of the NodeMCU
 * - Add visual indicator on one of the build in LEDs showing low-bat status
 * =============================================================================
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <EEPROM.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <time.h>
//#include <ArduinoJson.h>
#include "esp8266_ap.h"

// Define a web server at port 80 for HTTP
ESP8266WebServer *server;

// Define a JSON parser object.
//StaticJsonDocument<ALLOCATED_EEPROM> jsonBuffer;

// The BME680 Object on I2C bus.
Adafruit_BME680 bme;


/* This is where we declare the SRAM configuration parameters */
_settings settings;
int netCount=0;
unsigned long upTimeCounter = 0;
unsigned long lastMillis = 0;
byte bootCounter;
boolean measurementPublished = false;
time_t lastMeasurement;
char currentIP[16];

/*
 * Below function is linked to the end-poiunt that should serve the CSS file
 * (defined above) to the caller. It should return the const char containing
 * the CSS to be applied to the HTML page.
 */
void handleCSS() {
  server->send ( 200, PSTR("text/html"), CSS_FILE );
}

/*
 * Below function is linked to the end-point that should serve the JavaScript
 * file (also defined above) to the caller. It should return the const chart
 * containing the JS file (content) for the HTML page.
 */
void handleJS() {
  server->send ( 200, PSTR("text/html"), JS_SCIPT );
}

/*
 * Below function is called when the user clicks the "Scan" button
 * in the access point list. The function start a scan for access
 * point and returns a JSON with the availabe access point to
 * the client. The client then renders these in the list with
 * access points and preselects the current access point the
 * nodeMCU is connected to (if any).
 */
void handleAPScan() {
  
  scanAccessPoints();
  
  String accessPointList = getSTAList(settings.stationSSID);

  snprintf( html, HTML_PAGE_BUFFER_SIZE, "{%s}\n", accessPointList.c_str() );
  
  server->send ( 200, PSTR("application/json"), html );
  
}

/*
 * The below function parses an IP address provided as String
 * object intio a byte array (char).
 */
void parseIPAddress(String ip, byte *ipAddress) {
  
  char tmpIP[4];
  String digit = "";
  int i =0;
  int j = 0;

  while(j<4 && i<(ip.length()+1)) {
    while(ip.charAt(i)!='.' && i<(ip.length()+1)) {
      digit += ip.charAt(i);
      i++;
    }
    tmpIP[j] = (byte)digit.toInt();
    digit="";
    j++;
    i++;
  }
  
  ipAddress[0] = tmpIP[0];
  ipAddress[1] = tmpIP[1];
  ipAddress[2] = tmpIP[2];
  ipAddress[3] = tmpIP[3];
}

/*
 * Below function is mapped to the end-point "/" (root). It serves
 * the main configuration page and returns the HTML page defined in
 * the const char INDEX_HTML. This page is also called when the user
 * clicks the SUBMIT button on the page. All submitted values are 
 * parsed here from the body object and configuration changes are
 * stored by calling the writeConfig function.
 * After that the readConfig is called to actuate the stored conf-
 * figuration changes and apply them to the application.
 */
void handleRoot() {

  /*
   * CHeck if authentication is required. If so compare
   * the provided credentials against the configured
   * credential and return the result.
   */
  if(settings.stationRequireAuthentication) {
    if (!server->authenticate(settings.stationUsername, settings.stationPassword)) {
      return server->requestAuthentication();
    }
  }

  /*
   * Check if there is form data submitted to the NodeMCU. We
   * defined the submit element, if it is present in the body
   * we know/assume the form has been submitted by the user
   * and we will attempt to parse as many elements as possible
   * from it and change the configuration accordingly.
   */
  if (server->hasArg(PSTR("publishingPolicy"))) {

    settings.accessPointMode = (server->arg(PSTR("accessPointMode")).compareTo(PSTR("on"))==0);
    server->arg(PSTR("accessPointSSID")).toCharArray(settings.accessPointSSID, sizeof(settings.accessPointSSID));
    server->arg(PSTR("accessPointPassword")).toCharArray(settings.accessPointPassword, sizeof(settings.accessPointPassword));
    /*
     * Handling the IP address requires some additional attention. The IP
     * is human readable and provided as string. We need to split it by
     * seperating it at the dots and convert each individual String to
     * a byte value for storage.
     */
    Serial.println(server->arg(PSTR("accessPointIPAddress")));
    parseIPAddress(server->arg(PSTR("accessPointIPAddress")), settings.accessPointIP);
  
    settings.stationMode = (server->arg(PSTR("stationMode")).compareTo(PSTR("on"))==0);
    server->arg(PSTR("stationPassword")).toCharArray(settings.stationAccessPointPassword, sizeof(settings.stationAccessPointPassword));
    
    server->arg(PSTR("accessPointList")).toCharArray(settings.stationSSID, sizeof(settings.stationSSID));

    settings.stationRequireAuthentication = (server->arg(PSTR("requireAuthentication")).compareTo(("on"))==0);
    server->arg(PSTR("authenticationUsername")).toCharArray(settings.stationUsername, sizeof(settings.stationUsername));
    server->arg(PSTR("authenticationPassword")).toCharArray(settings.stationPassword, sizeof(settings.stationPassword));
    
    settings.sensorSampleInterval = server->arg(PSTR("sampleInterval")).toInt();
    
    server->arg(PSTR("stationHostname")).toCharArray(settings.hostName, sizeof(settings.hostName));
    server->arg(PSTR("publishingPolicy")).toCharArray(settings.publishingPolicy, sizeof(settings.publishingPolicy));

    server->arg(PSTR("publishURL")).toCharArray(settings.publishingURL, sizeof(settings.publishingURL));
    server->arg(PSTR("publishingUsername")).toCharArray(settings.publishingUsername, sizeof(settings.publishingUsername));
    server->arg(PSTR("publishingPassword")).toCharArray(settings.publishingPassword, sizeof(settings.publishingPassword));
    
    settings.useNTP = (server->arg(PSTR("useNTP")).compareTo(PSTR("on"))==0);
    settings.NTPOffset = server->arg(PSTR("NTPOffset")).toInt();
    server->arg(PSTR("NTPPoolURL")).toCharArray(settings.NTPPoolURL, sizeof(settings.NTPPoolURL));

    settings.serverPort = server->arg(PSTR("serverPort")).toInt();
    
    writeConfig();
    readConfig();
    //ESP.restart(); //ESP.reset();
  }
  /*
   * If form data was submitted is it now strored in the EEPROM
   * momory. We can now return the HTML page to the client and
   * let it query the settings etc.
   * For some reason the CSS won't be applied properly when
   * the link tag is applied. Could be something with char
   * set. For now we just insert it into the HTML page.
   * (The script does work when the src is specified in the
   * SCRIPT tag. Be sure to use the defer attribute otherwise
   * the script starts executing immediatly and will crash.)
   */
  snprintf(html, HTML_PAGE_BUFFER_SIZE, INDEX_HTML, CSS_FILE );
  server->send ( 200, PSTR("text/html"), html );
}

/*
 * Below function handles constructing a JSON containing
 * the currently valid settings. The settings are store
 * as a JSON in EEPROM memory and copied into SRAM memory
 * for use. The function copies the SRAM variables into
 * the JSON and returns it to the web-form.
 */
void handleSettings() {

  String accessPointList = getSTAList(settings.stationSSID);
  String channelList = getChannelList(settings.accessPointChannel);

  char pollingURL[200];
  snprintf(pollingURL, sizeof(pollingURL), "http://%s:%d%s", currentIP, settings.serverPort, pollURL);
  
  snprintf( html, HTML_PAGE_BUFFER_SIZE, JSON_SETTINGS,
    settings.accessPointMode?"true":"false",
    settings.accessPointSSID,
    settings.accessPointPassword,
    IPAddress(settings.accessPointIP[0], settings.accessPointIP[1], settings.accessPointIP[2], settings.accessPointIP[3]).toString().c_str(),
    settings.stationMode?"true":"false",
    accessPointList.c_str(),
    settings.stationAccessPointPassword,
    settings.stationRequireAuthentication?"true":"false",
    settings.stationUsername,
    settings.stationPassword,
    settings.sensorSampleInterval,
    settings.publishingURL,
    settings.publishingUsername,
    settings.publishingPassword,
    pollingURL,
    strcmp(settings.publishingPolicy, "Push")==0?"true":"false",
    strcmp(settings.publishingPolicy, "Poll")==0?"true":"false",
    settings.hostName,
    channelList.c_str(),
    settings.useNTP?"true":"false",
    settings.NTPOffset,
    settings.NTPPoolURL,
    settings.serverPort
  );

  server->send ( 200, PSTR("application/json"), html );

}

/*
 * Below function is linked to end-point /sensor/read and
 * triggers a read from the BME680. After/when the read
 * is completed the function constructs a JSON containing
 * the measurement results.
 */
void handleSensorRead() {

  if(settings.stationRequireAuthentication) {
    if (!server->authenticate(settings.stationUsername, settings.stationPassword)) {
      return server->requestAuthentication();
    }
  }
  
  if (! bme.performReading()) {
    Serial.println(PSTR("Failed to perform reading"));
    time_t now = time(nullptr);
    snprintf(html, HTML_PAGE_BUFFER_SIZE, SENSOR_HTML, settings.hostName, now, bme.temperature, bme.humidity, bme.pressure / 100.0, bme.gas_resistance, 10, PSTR("Failed to perform reading"));
  } else {
    lastMeasurement = time(nullptr);
    snprintf(html, 10000, SENSOR_HTML, settings.hostName, lastMeasurement, bme.temperature, bme.humidity, bme.pressure / 100.0, bme.gas_resistance, 0, PSTR("Success"));
  }
  server->send ( 200, PSTR("application/json"), html );
}

/*
 * Below function returns the current sensor values without
 * performing a new measurement. It is linked to end-point
 * /sensor/data
 */
void handleStatus() {

  unsigned long t = upTimeCounter/1000;
  unsigned long s = t%60;
  t=t/60;
  unsigned long m = t%60;
  t=t/60;
  unsigned long h = t%24;
  t=t/24;
  
  time_t n = time(nullptr);

  char ip[16];
  WiFi.localIP().toString().toCharArray(ip, sizeof(ip));
  
  snprintf(html, 10000, STATUS_HTML, t, h, m, s, ctime(&n), ctime(&lastMeasurement), settings.hostName, ip, bme.temperature, bme.humidity, bme.pressure / 100.0, bme.gas_resistance/1000);
  
  server->send ( 200, PSTR("test/html"), html );
}

/*
 * Below function constructs a JSON array containing the available
 * access point. The list is constructed based on the currently
 * known access-point. The function does not peroform a AP scan.
 */
String getSTAList(String selectItem) {

  String stationList = PSTR(" \"accessPointList\": [\n");

  for(int i=0; i<netCount; i++) {

    char s[WiFi.SSID(i).length()+1];
    WiFi.SSID(i).toCharArray(s, sizeof(s));
    
    stationList += PSTR("   {\"value\":\"") + WiFi.SSID(i) + PSTR("\", \"text\":\"") + WiFi.SSID(i) + PSTR("\", \"selected\": ") + (selectItem.compareTo(WiFi.SSID(i))==0?PSTR("true"):PSTR("false")) + PSTR("}") + (i+1<netCount?PSTR(",\n"):PSTR("\n"));
  }

  stationList += " ]\n";
  
  return stationList;
}

/*
 * Below function constructs a JSON array containing the
 * available frequencies/channels available on the NodeMCU.
 */
String getChannelList(byte Channel) {

  String chsnnelList = PSTR(" \"channelList\": [\n");
  
  for(int i=0; i<CH_COUNT; i++) {
    chsnnelList += PSTR("   {\"value\":\"") + String(i) + PSTR("\", \"text\":\"") + channels[i] + PSTR("\", \"selected\": ") + (i==Channel?PSTR("true"):PSTR("false")) + "}" + (i+1<CH_COUNT?PSTR(",\n"):PSTR("\n"));
  }
  chsnnelList += PSTR(" ]\n");
  
  return chsnnelList;
}

/*
 * Below function handles requests to unknown end-point.
 */
void handleNotFound() {
  digitalWrite ( LED_BUILTIN, 0 );
  String message = PSTR("File Not Found");
  message += PSTR("URI: ");
  message += server->uri();
  message += PSTR("Method: ");
  message += ( server->method() == HTTP_GET ) ? PSTR("GET") : PSTR("POST");
  message += PSTR("Arguments: ");
  message += server->args();
  message += PSTR("");

  for ( uint8_t i = 0; i < server->args(); i++ ) {
    message += PSTR(" ") + server->argName ( i ) + PSTR(": ") + server->arg ( i ) + PSTR("");
  }

  server->send ( 404, PSTR("text/plain"), message );
}


/*
 * Below function stores/overwrites the EEPROM settings
 * with the factory defaults.
 */
void storeFactoryDefaults() {

  Serial.printf(PSTR("Clean EEPROM before storing factory defaults.\n"));
  for(int address=0; address<ALLOCATED_EEPROM; address++) {
    EEPROM.write(address, 0);
  }
  EEPROM.write(0, NON_FACTORY);
  Serial.printf(PSTR("Resetting boot counter to zero.\n"));
  EEPROM.write(1, 0);
  EEPROM.put(2, factoryDefaults);
  EEPROM.commit();
  
  Serial.println(PSTR("factory default persisted."));
}

/*
 * Below function reads the stored settings from the EEPROM.
 * It also checks if the current settings are in custom mode
 * or factory default mode. If the settings are not in custom
 * mode the factory defaults are written to EEPROM. This is
 * done so to enable settings the factory defaults at initial
 * start-up after the module has been flashed.
 */
void readConfig() {

  Serial.println(PSTR("Reading configuration from EEPROM."));
  byte value = 0;
  
  // Read first byte. If !NON_FACTORY then first call 
  // storeFactoryDefaults to initialize the configuration
  value = EEPROM.read(0);
  Serial.printf(PSTR("Factory indicator value: %d\n"), value);
  if(value!=NON_FACTORY) {
    storeFactoryDefaults();
  }
  // Address zero if the factory indicator.
  // Address one is for the boot counter.
  EEPROM.get(2, settings);
  Serial.println(PSTR("Configuration loaded"));
  printConfig();
}

/*
 * Below function writes the current SRAM configuration
 * parameters to EEPROM.
 */
void writeConfig() {
  Serial.println(PSTR("Writting settings to EEPROM."));
  EEPROM.write(0, NON_FACTORY);
  EEPROM.put(2, settings);
  EEPROM.commit();
  Serial.println(PSTR("Writting settings to EEPROM finished."));
  readConfig();
}

/*
 * Below function is scafolding, it just prints the current
 * SRAM configuration to Serial port. When debugging you can
 * hence see what the current configuration is.
 */
void printConfig() {
  Serial.println(PSTR("------------------------------------------------------"));
  Serial.printf(PSTR("Configured server port: %d\n"), settings.serverPort);
  Serial.printf(PSTR("Configured SSID: %s\n"), settings.accessPointSSID);
  Serial.printf(PSTR("Configured AP Password: %\n"), settings.accessPointPassword);
  char Ip[15];
  IPAddress(settings.accessPointIP[0], settings.accessPointIP[1], settings.accessPointIP[2], settings.accessPointIP[3]).toString().toCharArray(Ip, 15);
  Serial.printf(PSTR("Configured AP IP: %s\n"), Ip);
  Serial.printf(PSTR("Configured APMode: %s\n"), settings.accessPointMode?"true":"false");
  Serial.printf(PSTR("Configured channel: %s\n"), channels[settings.accessPointChannel]);
  Serial.printf(PSTR("Configured STAMode: %s\n"), settings.stationMode?"true":"false");
  Serial.printf(PSTR("Configured STA SSID: %s\n"), settings.stationSSID);
  Serial.printf(PSTR("Configured STAPassword: %s\n"), settings.stationAccessPointPassword);
  Serial.printf(PSTR("Configures STA Hostname: %s\n"), settings.hostName);
  Serial.printf(PSTR("Configured Auth: %s\n"), settings.stationRequireAuthentication?"true":"false");
  Serial.printf(PSTR("Configured AuthUsername: %s\n"), settings.stationUsername);
  Serial.printf(PSTR("Configured AuthPassword: %s\n"), settings.stationPassword);
  Serial.printf(PSTR("Manual Sampling interval: %d\n"), settings.sensorSampleInterval);
  Serial.printf(PSTR("Configured publishing policy: %s\n"), settings.publishingPolicy);
  Serial.printf(PSTR("Configured push URL: %s\n"), settings.publishingURL);
  Serial.printf(PSTR("Configured push Username: %s\n"), settings.publishingUsername);
  Serial.printf(PSTR("Configured push Password: %s\n"), settings.publishingPassword);
  Serial.printf(PSTR("Configured use NTP: %s\n"), settings.useNTP?"true":"false");
  Serial.printf(PSTR("Configured NTP offset: %d\n"), settings.NTPOffset);
  Serial.printf(PSTR("Configured NTP pool URL: %s\n"), settings.NTPPoolURL);
  Serial.println(PSTR("------------------------------------------------------"));  
}

/*
 * This function has the nodeMCU scan for available access point.
 * The access point are available for listing onced the scan
 * is finished.
 */
void scanAccessPoints() {
  
  Serial.println(PSTR("Scanning for WiFi access points."));
  netCount = WiFi.scanNetworks();
  Serial.printf(PSTR("%d networks available.\n"), netCount);
  for(int i=0; i<netCount; i++) {
    Serial.printf(PSTR("%d: %s, Ch:%d (%ddBm) %s\n"), i + 1, WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i), WiFi.encryptionType(i) == ENC_TYPE_NONE ? PSTR("open") : PSTR("secured"));
  }
    
}

/*
 * Setup the module on boot.
 */
void setup() {

  Serial.begin(115200);
  Serial.println(PSTR("Settings up-time and checking boot-counter."));
  // Store last millis, we will use it later to reset the
  // boot counter.
  lastMillis = millis();
  upTimeCounter = 0;
  
  Serial.println(PSTR("Allocating EEPROM memory for configuration parameters."));
  EEPROM.begin(ALLOCATED_EEPROM);

  // Get the boot counter from EPROM, increment it and store it
  // back into the EPROM.
  bootCounter = EEPROM.read(1);
  bootCounter++;
  Serial.printf(PSTR("Boot counter incremented to %d\n"), bootCounter);

  if(bootCounter == 3) {
    // Boot counter hot three. We will reset to factory default and
    // reset the boot counter to zero.
    Serial.println(PSTR("Boot counter hit three, restoring factory defaults."));
    bootCounter = 0;
    storeFactoryDefaults();
  }
  
  EEPROM.write(1, bootCounter);
  EEPROM.commit();

  scanAccessPoints();
  
  readConfig();

  // Set-up a webserver on the configured port.
  //delete server;
  server = new ESP8266WebServer(settings.serverPort);
  
  if(settings.accessPointMode) {
    Serial.println(PSTR("Configuring access point..."));
    
    //set-up the custom IP address
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(IPAddress(settings.accessPointIP[0], settings.accessPointIP[1], settings.accessPointIP[2], settings.accessPointIP[3]), 
                      IPAddress(settings.accessPointIP[0], settings.accessPointIP[1], settings.accessPointIP[2], settings.accessPointIP[3]), 
                      IPAddress(255, 255, 255, 0));   // subnet FF FF FF 00  
    
    WiFi.softAP(settings.accessPointSSID, settings.accessPointPassword, settings.accessPointChannel, 0);
    IPAddress myIP = WiFi.softAPIP();
    myIP.toString().toCharArray(currentIP, sizeof(currentIP));
    Serial.printf(PSTR("AP IP address: %s\n"), currentIP);
    
  } else if(settings.stationMode && strcmp(settings.stationSSID, "")!=0) {
    WiFi.mode(WIFI_STA);
    Serial.printf(PSTR("Connecting to '%s' with password '%s'\n"), settings.stationSSID, settings.stationAccessPointPassword);
    WiFi.hostname(settings.hostName);
    
    WiFi.begin(settings.stationSSID, settings.stationAccessPointPassword);
    // Wait for connection
    int i = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(PSTR("."));
      i++;
      if(i%30==0){
        Serial.println();
      }
    }
    Serial.println();
    
    WiFi.localIP().toString().toCharArray(currentIP, sizeof(currentIP));
    
    Serial.printf(PSTR("Connected to: %s\n"), settings.stationSSID);
    Serial.printf(PSTR("IP address: %s\n"), currentIP);
    Serial.printf(PSTR("Hostname: %s\n"), settings.hostName);

    if(settings.useNTP) {
      Serial.println(PSTR("Setting up NTP support."));
      configTime(settings.NTPOffset, 0, settings.NTPPoolURL);
      Serial.println(PSTR("\nWaiting for time"));
      while (!time(nullptr)) {
        Serial.print(".");
        delay(500);
      }
    }
  } else {
    // Config error, neither station or ap mode has been specified, restore factory defaults and reboot.
    storeFactoryDefaults();
    ESP.restart();
  }
  
  Serial.println(PSTR("Attempting to initialize BME680"));
  if (!bme.begin()) {
    Serial.println(PSTR("Could not find a valid BME680 sensor, check BME680 address and/or check wiring!"));
    while (1);
  }
  
  server->on (PSTR("/"), handleRoot );
  server->on (PSTR("/sensor/read"), handleSensorRead);
  server->on (PSTR("/status"), handleStatus);
  server->on (PSTR("/settings"), handleSettings);
  server->on (PSTR("/aplist"), handleAPScan);
  server->on (PSTR("/nodemcu.css"), handleCSS);
  server->on (PSTR("/nodemcu.js"), handleJS);
  server->onNotFound ( handleNotFound );
  
  server->begin();
  Serial.println(PSTR("HTTP server started"));
  
  lastMillis = 0;
  upTimeCounter = 0;
}

/*
 * Main processing loop.
 */
void loop() {

  // Check the elapsed millis. If they hit over 600000
  // then we reset the bootCounter to zero.
  if(bootCounter!=0 && millis()>60000) {
    Serial.println(PSTR("Resetting boot counter to zero."));
    bootCounter = 0;
    EEPROM.write(1, bootCounter);
    EEPROM.commit();
  }
  
  server->handleClient();
  
  // Up-time counter processing.
  if(millis() >= lastMillis) {
    upTimeCounter += (millis() - lastMillis);
  } else {
    // millis cycled round and started at zero again.
    // We do not know exactly when the millis cycle round.
    // The millis are unsigned long which should be more
    // than enough to hold decades, but ... it doesn't.
    // We will lose some millis but that is not really
    // important.
    upTimeCounter += millis();
  }
  lastMillis = millis();
  
  // Get time.
  time_t now = time(nullptr);

  // Take measurements.
  // Do this continueally. The BME680 has
  // a burn in period at least 5 minutes.
  bme.performReading();
  
  // Check if it is time to publish a measurement.
  if(now%settings.sensorSampleInterval==0) {
    if(!measurementPublished || (settings.sensorSampleInterval==1 && now!=lastMeasurement)) {
      measurementPublished = true;
      lastMeasurement = time(nullptr);
      Serial.println(ctime(&lastMeasurement));
      Serial.print(PSTR("Temperature = ")); Serial.print(bme.temperature); Serial.println(PSTR(" *C"));
      Serial.print(PSTR("Pressure = ")); Serial.print(bme.pressure / 100.0); Serial.println(PSTR(" hPa"));
      Serial.print(PSTR("Humidity = ")); Serial.print(bme.humidity); Serial.println(PSTR(" %"));
      Serial.print(PSTR("Gas = ")); Serial.print(bme.gas_resistance / 1000.0); Serial.println(PSTR(" KOhms"));

      if(strcmp(settings.publishingPolicy, PSTR("Push"))==0) {
        HTTPClient http;    //Declare object of class HTTPClient
        
        http.begin(settings.publishingURL);      //Specify request destination
        http.addHeader(PSTR("Content-Type"), PSTR("application/json"));  //Specify content-type header

        snprintf(html, 500, SENSOR_HTML, settings.hostName, lastMeasurement, bme.temperature, bme.humidity, bme.pressure / 100.0, bme.gas_resistance, 0, PSTR("Success"));
        Serial.println(html);
        int httpCode = http.POST(html);     //Send the request
        String payload = http.getString();  //Get the response payload

        // On http code 200 the remote server accepted the
        // measurement.
        measurementPublished = (httpCode==200);
        
        Serial.println(httpCode);   //Print HTTP return code
        Serial.println(payload);    //Print request response payload
        
        http.end();  //Close connection
      }
    }
  } else {
    measurementPublished = false;
  }
  
}
