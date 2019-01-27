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
#include <ArduinoJson.h>

#define SEALEVELPRESSURE_HPA (1013.25)
#define SI_COUNT 12
#define CH_COUNT 14
#define HTML_PAGE_BUFFER_SIZE 12000

const byte NON_FACTORY = 22;
const int ALLOCATED_EEPROM = 1024;

/* These are the factory default values */
char factorySSID[] PROGMEM = "ESP8266";
char factoryAPPassword[] PROGMEM = "ESP8266Test";
char factoryIP[] = {192, 168, 4, 1};
char factoryAPMode[] PROGMEM = "true";
char factorySTAMode[] PROGMEM = "false";
char factoryAuth[] PROGMEM = "true";
char factoryAuthUsername[] PROGMEM = "admin";
char factoryAuthPassword[] PROGMEM = "admin";
int factorySI PROGMEM = 10;
byte factoryChannel PROGMEM = 0;
char factoryPublishPolicy[] PROGMEM = "Poll";
char factoryUseNTP[] PROGMEM = "true";
int factoryNTPOffset PROGMEM = 0;
char factoryNTPPoolURL[] PROGMEM = "nl.pool.ntp.org";
char channels[CH_COUNT][14] = {"1 - 2412 MHz", "2 - 2417 MHz", "3 - 2422 MHz", "4 - 2427 MHz", "5 - 2432 MHz", "6 - 2437 MHz", "7 - 2442 MHz", "8 - 2447 MHz", "9 - 2452 MHz", "10 - 2457 MHz", "11 - 2462 MHz", "12 - 2467 MHz", "13 - 2472 MHz", "14 - 2484 MHz"};

char pollURL[] PROGMEM = "/sensor/read";

/*
 * Below const char is contains the JSON structure for
 * delivering the settings to the web-client. It will
 * be used to populate the form data.
 */
const char JSON_SETTINGS[] PROGMEM = 
  "{\n"
  " \"accessPointMode\": %s,\n"
  " \"accessPointSSID\": \"%s\",\n"
  " \"accessPointPassword\": \"%s\",\n"
  " \"accessPointIPAddress\": \"%s\",\n"
  " \"stationMode\": %s,\n"
  " %s,\n"
  " \"stationPassword\": \"%s\",\n"
  " \"requireAuthentication\": %s,\n"
  " \"authenticationUsername\": \"%s\",\n"
  " \"authenticationPassword\": \"%s\",\n"
  " \"sampleInterval\": %d,\n"
  " \"publishURL\": \"%s\",\n"
  " \"publishingUsername\": \"%s\",\n"
  " \"publishingPassword\": \"%s\",\n"
  " \"pollURL\": \"%s\",\n"
  " \"publishingPolicy\":{\"Push\": %s, \"Poll\": %s},\n"
  " \"stationHostname\": \"%s\",\n"
  " %s,\n"
  " \"useNTP\": %s,\n"
  " \"NTPOffset\": %d,\n"
  " \"NTPPoolURL\": \"%s\"\n"
  "}\n";

/*
 * Below const char defines the JavaScript to be loaded by the HTML poage.
 * The code contains routines to load the form data (Ajax) from the NodeMCU
 * and populate the form fields. The form data is provided as JSON and is
 * parsed by the script.
 * Further the script contains a function to periodically retreive the
 * status information from the NodeMCU and display it on the web-page.
 * This is done every 10 seconds.
 */
const char JS_SCIPT[] PROGMEM =
  "function openConfigSheet(evt, cityName) {\n"
  "  var i, tabcontent, tablinks;\n"
  " tabcontent = document.getElementsByClassName(\"tabcontent\");\n"
  " for (i = 0; i < tabcontent.length; i++) {\n"
  "   tabcontent[i].style.display = \"none\";\n"
  " }\n"
  " tablinks = document.getElementsByClassName(\"tablinks\");\n"
  " for (i = 0; i < tablinks.length; i++) {\n"
  "   tablinks[i].className = tablinks[i].className.replace(\"active\", \"\");\n"
  " }\n"
  " document.getElementById(cityName).style.display = \"block\";\n"
  " evt.currentTarget.className += \" active\";\n"
  "}\n"
  "\n"
  "var x = setInterval(function() {loadData(\"/sensor/read\", \"status\", updateData)}, 10000);\n"
  "\n"
  "function loadData(url, element, callback){\n"
  " var xhttp = new XMLHttpRequest();\n"
  " xhttp.onreadystatechange = function(){\n"
  "   if(this.readyState == 4 && this.status == 200){\n"
  "     callback.apply({xhttp: xhttp, element:element});\n"
  "   }\n"
  " };\n"
  " xhttp.open(\"GET\", url, true);\n"
  " xhttp.send();\n"
  "}\n"
  "\n"
  "function updateData(){\n"
  " document.getElementById(this.element).innerHTML = this.xhttp.responseText;\n"
  "}\n"
  "\n"
  "function getSettings(url) {\n"
  " var xhttp = new XMLHttpRequest();\n"
  "\n"
  " xhttp.onreadystatechange = function(){\n"
  "   if(this.readyState == 4 && this.status == 200){\n"
  "     setFormData.apply(xhttp);\n"
  "   }\n"
  " };\n"
  " xhttp.open(\"GET\", url, true);\n"
  " xhttp.send();\n"
  "}\n"
  "\n"
  "function setFormData() {\n"
  " \n"
  " var obj = JSON.parse(this.responseText);\n"
  " \n"
  " for(var key in obj) {\n"
  "   var element = document.getElementById(key);\n"
  "   if(element == null) {\n"
  "     var elements = document.getElementsByName(key);\n"
  "     for(i=0; i<elements.length; i++) {\n"
  "       elements[i].checked = obj[key][elements[i].value];\n"
  "     }\n"
  "   } else {\n"
  "     if(element.type == 'select-one') {\n"
  "       while(element.length > 0) {\n"
  "         element.remove(0);\n"
  "       }\n"
  "       for(i=0; i<obj[key].length; i++) {\n"
  "         var option = document.createElement('option');\n"
  "         option.value = obj[key][i].value;\n"
  "         option.text = obj[key][i].text;\n"
  "         element.add(option);\n"
  "         if(obj[key][i].selected==true) {\n"
  "           element.selectedIndex = i;\n"
  "         }\n"
  "       }\n"
  "     } else if(element.type == 'checkbox') {\n"
  "       element.checked = obj[key];\n"
  "     } else {\n"
  "       element.value = obj[key];\n"
  "     }\n"
  "   }\n"
  " }\n"
  " \n"
  "}\n";

/*
 * Below const char defines the CCS to be used by the HTML page.
 * The HTML is requested asynchronous from the NodeMCU.
 */
const char CSS_FILE[] PROGMEM = 
  "@charset \"UTF-8\";\n"
  "     /* Style the tab */\n"
  "     .tab {\n"
  "       overflow: hidden;\n"
  "       border: 1px solid #ccc;\n"
  "       background-color: #f1f1f1;\n"
  "       border-radius: 5px;\n"
  "       padding: 5px;\n"
  "     }\n"
  "\n"
  "     /* Style the buttons that are used to open the tab content */\n"
  "     .tab button {\n"
  "       background-color: inherit;\n"
  "       float: left;\n"
  "       border: 1px solid black;\n"
  "       outline: none;\n"
  "       cursor: pointer;\n"
  "       padding: 14px 16px;\n"
  "       transition: 0.3s;\n"
  "       border-radius: 10px;\n"
  "       margin: 5px;\n"
  "     }\n"
  "\n"
  "     /* Change background color of buttons on hover */\n"
  "     .tab button:hover {\n"
  "       background-color: #ddd000;\n"
  "     }\n"
  "\n"
  "     /* Create an active/current tablink class */\n"
  "     .tab button.active {\n"
  "       background-color: #ccc000;\n"
  "     }\n"
  "\n"
  "     /* Style the tab content */\n"
  "     .tabcontent {\n"
  "       display: none;\n"
  "       padding: 6px 12px;\n"
  "       border: 1px solid #ccc;\n"
  "       border-top: none;\n"
  "     }\n"
  "     /* Table style */\n"
  "     .table {\n"
  "       border: 1px solid black;\n"
  "       border-radius: 5px;\n"
  "       padding: 5px;\n"
  "       margin: 5px;\n"
  "     }\n"
  "     .table td {\n"
  "       border-bottom: 1px solid red;\n"
  "       vertical-align: text-top;\n"
  "     }\n"
  "     \n"
  "     .banner {\n"
  "       border-radius: 15px 50px;\n"
  "       border: 1px solid green;\n"
  "       padding: 5px;\n"
  "       margin: 5px;\n"
  "       text-align: center;\n"
  "       background-color: green;\n"
  "     }\n"
  "     .banner h1 {\n"
  "       color: #ccc000;\n"
  "     }\n";

/*
 * The below const char contains the main HTML setup page. There
 * are references to the CSS page and the JavaScript page in the
 * HTML tages STYLE and SCRIPT.
 */
const char INDEX_HTML[] PROGMEM =
  "<!DOCTYPE HTML>\n"
  "<HTML>\n"
  "  <HEAD>\n"
  "   <TITLE>NodedMCU Configuration</TITLE>\n"
  "   <style>%s</style>\n"
  "   <script defer=\"defer\" src=\"/nodemcu.js\"></script>\n"
  " <BODY onload=\"getSettings('/settings');\">\n"
  "   <div class=\"banner\">\n"
  "     <h1>MCS - NodeMCU BME680 setup</h1>\n"
  "   </div>\n"
  "   <div class=\"tab\">\n"
  "     <button class=\"tablinks\" onclick=\"openConfigSheet(event, 'AP')\">Access Point</button>\n"
  "     <button class=\"tablinks\" onclick=\"openConfigSheet(event, 'STA')\">WiFi station</button>\n"
  "     <button class=\"tablinks\" onclick=\"openConfigSheet(event, 'Auth')\">Authentication</button>\n"
  "     <button class=\"tablinks\" onclick=\"openConfigSheet(event, 'BME680')\">BME680 Sensor</button>\n"
  "     <button class=\"tablinks\" onclick=\"openConfigSheet(event, 'Status')\">Status</button>\n"
  "   </div>\n"
  "   <FORM action=\"/\" method=\"POST\" >\n"
  "     <div id=\"AP\" class=\"tabcontent\">\n"
  "       <h3>Access Point configuration</h3>\n"
  "       <table class=\"table\">\n"
  "       <tr>\n"
  "         <td colspan=\"2\">\n"
  "           <input id=\"accessPointMode\" name=\"accessPointMode\" type=\"checkbox\">Operate as WiFi Access Point\n"
  "         </td>\n"
  "       </tr>\n"
  "       <tr>\n"
  "         <td>Access Point SSID:</td>\n"
  "         <td>\n"
  "           <input id=\"accessPointSSID\" name=\"accessPointSSID\" type=\"text\">\n"
  "         </td>\n"
  "       </tr>\n"
  "       <tr>\n"
  "         <td>Channel:</td>\n"
  "         <td>\n"
  "           <select id=\"channelList\" name=\"channelList\" width=\"200px\"></select>\n"
  "       </tr>\n"
  "       <tr>\n"
  "         <td>Access Point password:</td>\n"
  "         <td>\n"
  "           <input id=\"accessPointPassword\" name=\"accessPointPassword\" type=\"password\">\n"
  "         </td>\n"
  "       </tr>\n"
  "       <tr>\n"
  "         <td>IP Address:</td>\n"
  "         <td>\n"
  "           <input id=\"accessPointIPAddress\" name=\"accessPointIPAddress\" type=\"text\">\n"
  "       </tr>\n"
  "       </table>\n"
  "     </div>\n"
  "     <div id=\"STA\" class=\"tabcontent\">\n"
  "       <h3>WiFi Station configuration</h3>\n"
  "       <table class=\"table\">\n"
  "       <tr>\n"
  "         <td colspan=\"2\">\n"
  "           <input id=\"stationMode\" name=\"stationMode\" type=\"checkbox\">Operate as WiFi Station\n"
  "         </td>\n"
  "       </tr>\n"
  "       <tr>\n"
  "         <td>Available Access Points:</td>\n"
  "         <td>\n"
  "           <select id=\"accessPointList\" name=\"accessPointList\" size=\"10\" width=\"200px\"></select>\n"
  "       </tr>\n"
  "        <tr>\n"
  "          <td>Station hostname:</td>\n"
  "          <td>\n"
  "            <input id=\"stationHostname\" name=\"stationHostname\" type=\"text\">\n"
  "          </td>\n"
  "        </tr>\n"
  "       <tr>\n"
  "         <td colspan=\"2\"><input name=\"scan\" type=\"button\" value=\"Scan\" onclick=\"getSettings('/aplist');\"></td>\n"
  "       </tr>\n"
  "       <tr>\n"
  "         <td>Password:</td>\n"
  "         <td>\n"
  "           <input id=\"stationPassword\" name=\"stationPassword\" type=\"password\">\n"
  "         </td>\n"
  "       </tr>\n"
  "       <tr>\n"
  "         <td colspan=\"2\">\n"
  "           <input id=\"useNTP\" name=\"useNTP\" type=\"checkbox\">Use NTP time synchronisation.\n"
  "         </td>\n"
  "       </tr>\n"
  "        <tr>\n"
  "          <td>Offset:</td>\n"
  "          <td>\n"
  "            <input id=\"NTPOffset\" name=\"NTPOffset\" type=\"text\"> Seconds.\n"
  "          </td>\n"
  "        </tr>\n"
  "        <tr>\n"
  "          <td>NTP pool URL:</td>\n"
  "          <td>\n"
  "            <input id=\"NTPPoolURL\" name=\"NTPPoolURL\" type=\"text\">\n"
  "          </td>\n"
  "        </tr>\n"
  "       </table>\n"
  "     </div>\n"
  "     <div id=\"Auth\" class=\"tabcontent\">\n"
  "       <h3>Authentication</h3>\n"
  "       <table class=\"table\">\n"
  "       <tr>\n"
  "         <td colspan=\"2\">\n"
  "           <input id=\"requireAuthentication\" name=\"requireAuthentication\" type=\"checkbox\">Require authentication\n"
  "         </td>\n"
  "       </tr>\n"
  "       <tr>\n"
  "         <td>Username:</td>\n"
  "         <td>\n"
  "           <input id=\"authenticationUsername\" name=\"authenticationUsername\" type=\"text\">\n"
  "         </td>\n"
  "       </tr>\n"
  "       <tr>\n"
  "         <td>Password:</td>\n"
  "         <td>\n"
  "           <input id=\"authenticationPassword\" name=\"authenticationPassword\" type=\"password\">\n"
  "         </td>\n"
  "       </tr>\n"
  "       </table>\n"
  "     </div>    \n"
  "     <div id=\"BME680\" class=\"tabcontent\">\n"
  "       <h3>BME680 Sensor configuration</h3>\n"
  "       <table class=\"table\">\n"
  "       <tr>\n"
  "         <td>Sample interval:</td>\n"
  "         <td>\n"
  "           <input id=\"sampleInterval\" name=\"sampleInterval\" type=\"text\">Seconds\n"
  "         </td>\n"
  "       </tr>\n"
  "       <tr>\n"
  "         <td colspan=\"2\">Measurements publishing policy:</td>\n"
  "       </tr>\n"
  "       <tr>\n"
  "         <td>\n"
  "           <input name=\"publishingPolicy\" type=\"radio\" value=\"Push\">Push\n"
  "         </td>\n"
  "         <td>\n"
  "           <table>\n"
  "             <tr>\n"
  "               <td>POST to:</td>\n"
  "               <td>\n"
  "                 <input id=\"publishURL\" name=\"publishURL\" type=\"text\">\n"
  "               </td>\n"
  "             </tr>\n"
  "             <tr>\n"
  "               <td>Username:</td>\n"
  "               <td>\n"
  "                 <input id=\"publishingUsername\" name=\"publishingUsername\" type=\"text\">\n"
  "               </td>\n"
  "             </tr>\n"
  "             <tr>\n"
  "               <td>Password:</td>\n"
  "               <td>\n"
  "                 <input id=\"publishingPassword\" name=\"publishingPassword\" type=\"password\">\n"
  "               </td>\n"
  "             </tr>\n"
  "           </table>\n"
  "         </td>\n"
  "       </tr>\n"
  "       <tr>\n"
  "         <td>\n"
  "           <input name=\"publishingPolicy\" type=\"radio\" value=\"Poll\">Poll\n"
  "         </td>\n"
  "         <td>\n"
  "           <table>\n"
  "             <tr>\n"
  "               <td>GET from:</td>\n"
  "               <td>\n"
  "                 <input id=\"pollURL\" name=\"pollURL\" type=\"text\" readonly >\n"
  "               </td>\n"
  "             </tr>\n"
  "           </table>\n"
  "         </td>\n"
  "       </tr>\n"
  "       </table>\n"
  "     </div>\n"
  "     <div id=\"Status\" class=\"tabcontent\">\n"
  "       <h3>Status information</h3>\n"
  "       <div id=\"status\" height=\"200px\" width=\"200px\">\n"
  "       </div>\n"
  "     </div>\n"
  "     <input type=\"submit\" value=\"Submit\"> <input type=\"button\" value=\"Reset\" onclick=\"getSettings('/settings');\">\n"
  "   </FORM>\n"
  " </BODY>\n"
  "</HTML>\n";

const char SENSOR_HTML[] PROGMEM = "{\"timestamp\":%d, \"temperature\":%f, \"humidity\":%f, \"air-pressure\":%f, \"voc\":%f, \"resultCode\": %d, \"resultText\", \"%s\"}";

// Declare String to manipulate html data.
char html[HTML_PAGE_BUFFER_SIZE];
  
// Define a web server at port 80 for HTTP
ESP8266WebServer server(80);

// Define a JSON parser object.
StaticJsonDocument<ALLOCATED_EEPROM> jsonBuffer;

// The BME680 Object on I2C bus.
Adafruit_BME680 bme;

//WiFiUDP ntpUDP;
// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
//NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);


/* This is where we declare the SRAM configuration parameters */
String ssid;
String APPassword;
char APIP[4];
String APMode;
String STAMode;
String STAPassword;
String STASSID;
String Auth;
String AuthUsername;
String AuthPassword;
int SIManual;
String pushURL;
String pushUsername;
String pushPassword;
String publishPolicy;
String espHostname;
byte Channel;
int netCount=0;
String UseNTP;
int NTPOffset;
String NTPPoolURL;

boolean measurementTaken = false;
time_t lastMeasurement;

/*
 * Below function is linked to the end-poiunt that should serve the CSS file
 * (defined above) to the caller. It should return the const char containing
 * the CSS to be applied to the HTML page.
 */
void handleCSS() {
  server.send ( 200, "text/html", CSS_FILE );
}

/*
 * Below function is linked to the end-point that should serve the JavaScript
 * file (also defined above) to the caller. It should return the const chart
 * containing the JS file (content) for the HTML page.
 */
void handleJS() {
  server.send ( 200, "text/html", JS_SCIPT );
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
  
  String accessPointList = getSTAList(STASSID);

  char apl[accessPointList.length()+1];
  accessPointList.toCharArray(apl, sizeof(apl));
  
  snprintf( html, HTML_PAGE_BUFFER_SIZE, "{%s}\n", apl );
  
  server.send ( 200, "application/json", html );
  
}

/*
 * The below function parses an IP address provided as String
 * object intio a byte array (char).
 */
void parseIPAddress(String ip, char *ipAddress) {
  
  char tmpIP[4];
  String digit = "";
  int i =0;
  int j = 0;

  while(j<4 && i<(ip.length()+1)) {
    while(ip.charAt(i)!='.' && i<(ip.length()+1)) {
      digit += ip.charAt(i);
      i++;
    }
    Serial.println("digit=" + digit);
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
  if(Auth.compareTo("true")==0) {
    char u[AuthUsername.length()+1];
    AuthUsername.toCharArray(u, sizeof(u));
    char p[AuthPassword.length()+1];
    AuthPassword.toCharArray(p, sizeof(p));
    if (!server.authenticate(u, p)) {
      return server.requestAuthentication();
    }
  }

  /*
   * Check if there is form data submitted to the NodeMCU. We
   * defined the submit element, if it is present in the body
   * we know/assume the form has been submitted by the user
   * and we will attempt to parse as many elements as possible
   * from it and change the configuration accordingly.
   */
  if (server.hasArg("publishingPolicy")) {

    APMode = (server.arg("accessPointMode").compareTo("on")==0?"true":"false");
    ssid = server.arg("accessPointSSID");
    APPassword = server.arg("accessPointPassword");
    /*
     * Handling the IP address requires some additional attention. The IP
     * is human readable and provided as string. We need to split it by
     * seperating it at the dots and convert each individual String to
     * a byte value for storage.
     */
    Serial.println(server.arg("accessPointIPAddress"));
    parseIPAddress(server.arg("accessPointIPAddress"), APIP);
  
    STAMode = (server.arg("stationMode").compareTo("on")==0?"true":"false");
    STAPassword = server.arg("stationPassword");
    
    STASSID = server.arg("accessPointList");

    Auth = (server.arg("requireAuthentication").compareTo("on")==0?"true":"false");
    AuthUsername = server.arg("authenticationUsername");
    AuthPassword = server.arg("authenticationPassword");
    
    SIManual = server.arg("sampleInterval").toInt();
    
    espHostname = server.arg("stationHostname");
    publishPolicy = server.arg("publishingPolicy");

    pushURL = server.arg("publishURL");
    pushUsername = server.arg("publishingUsername");
    pushPassword = server.arg("publishingPassword");
    
    UseNTP = (server.arg("useNTP").compareTo("on")==0?"true":"false");
    NTPOffset = server.arg("NTPOffset").toInt();
    NTPPoolURL = server.arg("NTPPoolURL");
    
    writeConfig();
    readConfig();
    printConfig();
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
  server.send ( 200, "text/html", html );
}

/*
 * Below function handles constructing a JSON containing
 * the currently valid settings. The settings are store
 * as a JSON in EEPROM memory and copied into SRAM memory
 * for use. The function copies the SRAM variables into
 * the JSON and returns it to the web-form.
 */
void handleSettings() {

  String accessPointList = getSTAList(STASSID);
  String channelList = getChannelList(Channel);

  char apl[accessPointList.length()+1];
  accessPointList.toCharArray(apl, sizeof(apl));

  char chl[channelList.length()+1];
  channelList.toCharArray(chl, sizeof(chl));

  char IP[16];
  char ph[100];
  
  WiFi.localIP().toString().toCharArray(IP, sizeof(IP));
  snprintf( ph, sizeof(ph), "http://%s%s", IP, pollURL);
  
  String ip = IPAddress(APIP[0], APIP[1], APIP[2], APIP[3]).toString();
  ip.toCharArray(IP, sizeof(IP)+1);
  
  snprintf( html, HTML_PAGE_BUFFER_SIZE, JSON_SETTINGS,
    APMode.c_str(),
    ssid.c_str(),
    APPassword.c_str(),
    IP,
    STAMode.c_str(),
    apl,
    STAPassword.c_str(),
    Auth.c_str(),
    AuthUsername.c_str(),
    AuthPassword.c_str(),
    SIManual,
    pushURL.c_str(),
    pushUsername.c_str(),
    pushPassword.c_str(),
    ph,
    (publishPolicy.compareTo("Push")==0?"true":"false"),
    (publishPolicy.compareTo("Poll")==0?"true":"false"),
    espHostname.c_str(),
    chl,
    UseNTP.c_str(),
    NTPOffset,
    NTPPoolURL.c_str()
  );
  
  server.send ( 200, "application/json", html );

}

/*
 * Below function is linked to end-point /sensor/read and
 * triggers a read from the BME680. After/when the read
 * is completed the function constructs a JSON containing
 * the measurement results.
 */
void handleSensorRead() {

  if(Auth.compareTo("true")==0) {
    char u[AuthUsername.length()+1];
    AuthUsername.toCharArray(u, sizeof(u));
    char p[AuthPassword.length()+1];
    AuthPassword.toCharArray(p, sizeof(p));
    if (!server.authenticate(u, p)) {
      return server.requestAuthentication();
    }
  }

  if (! bme.performReading()) {
    Serial.println("Failed to perform reading");
    time_t now = time(nullptr);
    snprintf(html, HTML_PAGE_BUFFER_SIZE, SENSOR_HTML, now, bme.temperature, bme.humidity, bme.pressure / 100.0, bme.gas_resistance, 10, "Failed to perform reading");
  } else {
    lastMeasurement = time(nullptr);
    snprintf(html, 10000, SENSOR_HTML, lastMeasurement, bme.temperature, bme.humidity, bme.pressure / 100.0, bme.gas_resistance, 0, "Success");
  }
  server.send ( 200, "application/json", html );
}

/*
 * Below function returns the current sensor values without
 * performing a new measurement. It is linked to end-point
 * /sensor/data
 */
void handleSensorData() {

  snprintf(html, 10000, SENSOR_HTML, lastMeasurement, bme.temperature, bme.humidity, bme.pressure / 100.0, bme.gas_resistance, 0, "Success");
  
  server.send ( 200, "application/json", html );
}

/*
 * Below function constructs a JSON array containing the available
 * access point. The list is constructed based on the currently
 * known access-point. The function does not peroform a AP scan.
 */
String getSTAList(String selectItem) {

  String stationList = " \"accessPointList\": [\n";

  for(int i=0; i<netCount; i++) {

    char s[WiFi.SSID(i).length()+1];
    WiFi.SSID(i).toCharArray(s, sizeof(s));
    
    stationList += "   {\"value\":\"" + WiFi.SSID(i) + "\", \"text\":\"" + WiFi.SSID(i) + "\", \"selected\": " + (selectItem.compareTo(WiFi.SSID(i))==0?"true":"false") + "}" + (i+1<netCount?",\n":"\n");
  }

  stationList += " ]\n";
  
  return stationList;
}

/*
 * Below function constructs a JSON array containing the
 * available frequencies/channels available on the NodeMCU.
 */
String getChannelList(byte Channel) {

  String chsnnelList = " \"channelList\": [\n";;
  
  for(int i=0; i<CH_COUNT; i++) {
    chsnnelList += "   {\"value\":\"" + String(i) + "\", \"text\":\"" + channels[i] + "\", \"selected\": " + (i==Channel?"true":"false") + "}" + (i+1<CH_COUNT?",\n":"\n");
  }
  chsnnelList += " ]\n";
  
  return chsnnelList;
}

/*
 * Below function handles requests to unknown end-point.
 */
void handleNotFound() {
  digitalWrite ( LED_BUILTIN, 0 );
  String message = "File Not Found";
  message += "URI: ";
  message += server.uri();
  message += "Method: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "Arguments: ";
  message += server.args();
  message += "";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "";
  }

  server.send ( 404, "text/plain", message );
}

/*
 * Below function reads a variable from EEPROM.
 */
void readVar(char *var, int size, int *address) {
  for(int i=0; i<size; i++) {
    *var = EEPROM.read(*address+i);
    var++;
  }
  *address+=size;
}

/*
 * Below function writes a variable to EEPROM.
 */
void writeVar(char *var, int size, int *address) {
  for(int i=0; i<size; i++) {
    EEPROM.write(*address+i, *var);
    var++;
  }
  *address+=size;
}

/*
 * Below function stores/overwrites the EEPROM settings
 * with the factory defaults.
 */
void storeFactoryDefaults() {

  Serial.printf("Clean EEPROM before storing factory defaults.\n");
  for(int address=0; address<ALLOCATED_EEPROM; address++) {
    EEPROM.write(address, 0);
  }
  EEPROM.commit();
  
  int address = 0;
  
  Serial.printf("Storing factory default configuration.\n");
  
  ssid = String(factorySSID);
  APPassword = String(factoryAPPassword);
  APIP[0] = factoryIP[0];
  APIP[1] = factoryIP[1];
  APIP[2] = factoryIP[2];
  APIP[3] = factoryIP[3];
  APMode = String(factoryAPMode);
  STAMode = String(factorySTAMode);
  STAPassword = "";
  STASSID = "";
  Auth = String(factoryAuth);
  AuthUsername = String(factoryAuthUsername);
  AuthPassword = String(factoryAuthPassword);
  SIManual = factorySI;
  pushURL = "";
  pushUsername = "";
  pushPassword = "";
  publishPolicy = String(factoryPublishPolicy);
  espHostname = "";
  Channel = factoryChannel;
  UseNTP = String(factoryUseNTP);
  NTPOffset = factoryNTPOffset;
  NTPPoolURL = String(factoryNTPPoolURL);

  writeConfig();

  EEPROM.write(0, NON_FACTORY);
  EEPROM.commit();
  readConfig();
  printConfig();
  Serial.println("factory default persisted.");
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

  Serial.println("Reading configuration from EEPROM.");
  int address = 0;
  byte value = 0;
  // Read first byte. If !NON_FACTORY then first call 
  // storeFactoryDefaults to initialize the configuration
  
  value = EEPROM.read(address);
  Serial.printf("Factory indicator value: %d\n", value);
  if(value!=NON_FACTORY) {
    storeFactoryDefaults();
  }
  address++;
  
  readVar(html, ALLOCATED_EEPROM - 2, &address);
  Serial.printf("Retreived configuration JSON: %s\n", html);
  
  DeserializationError error = deserializeJson(jsonBuffer, html);
  
  if (error) {
    Serial.println(html);
    Serial.println("DeserializeJson() failed: ");
    Serial.println(error.c_str());
    storeFactoryDefaults();
    return;
  }
  
  JsonObject root = jsonBuffer.as<JsonObject>();
  
  APMode = ((boolean)root["accessPointMode"]?"true":"false");

  const char* a = root["accessPointSSID"];
  ssid = String(a);

  Channel = root["channel"];

  a = root["accessPointPassword"];
  APPassword = String(a);

  const char* ip = root["accessPointIPAddress"];
  parseIPAddress(String(ip), APIP);

  STAMode = ((boolean)root["stationMode"]?"true":"false");

  a = root["accessPoint"];
  STASSID = String(a);

  a = root["stationHostname"];
  espHostname = String(a);

  a = root["stationPassword"];
  STAPassword = String(a);

  Auth = ((boolean)root["requireAuthentication"]?"true":"false");

  a = root["authenticationUsername"];
  AuthUsername = String(a);

  a = root["authenticationPassword"];
  AuthPassword = String(a);

  SIManual = root["sampleInterval"];

  a = root["publishingPolicy"];
  publishPolicy = String(a);

  a = root["publishURL"];
  pushURL = String(a);

  a = root["publishingUsername"];
  pushUsername = String(a);

  a = root["publishingPassword"];
  pushPassword = String(a);

  UseNTP = ((boolean)root["useNTP"]?"true":"false");

  NTPOffset = root["NTPOffset"];

  a = root["NTPPoolURL"];
  NTPPoolURL = String(a);

}

/*
 * Below function writes the current SRAM configuration
 * parameters to EEPROM.
 */
void writeConfig() {

  int address = 1;

  JsonObject root = jsonBuffer.to<JsonObject>();

  root["accessPointMode"] = (APMode.compareTo("true")==0);
  
  root["accessPointSSID"] = ssid;

  root["channel"] = Channel;
  
  root["accessPointPassword"] = APPassword;
  
  root["accessPointIPAddress"] = IPAddress(APIP[0], APIP[1], APIP[2], APIP[3]).toString();

  root["stationMode"] = (STAMode.compareTo("true")==0);

  root["accessPoint"] = STASSID;

  root["stationHostname"] = espHostname;

  root["stationPassword"] = STAPassword;

  root["requireAuthentication"] = (Auth.compareTo("true")==0);

  root["authenticationUsername"] = AuthUsername;

  root["authenticationPassword"] = AuthPassword;
  
  root["sampleInterval"] = SIManual;

  root["publishingPolicy"] = publishPolicy;

  root["publishURL"] = pushURL;

  root["publishingUsername"] = pushUsername;

  root["publishingPassword"] = pushPassword;

  root["useNTP"] = (UseNTP.compareTo("true")==0);

  root["NTPOffset"] = NTPOffset;
  
  root["NTPPoolURL"] = NTPPoolURL;
  
  String a;
  serializeJson(root, a);
  Serial.println(a);
  a.toCharArray(html, ALLOCATED_EEPROM - 2);
  
  address = 1;
  writeVar(html, ALLOCATED_EEPROM - 2, &address);

  EEPROM.write(0, NON_FACTORY);
  EEPROM.commit();
  
}

/*
 * Below function is scafolding, it just prints the current
 * SRAM configuration to Serial port. When debugging you can
 * hence see what the current configuration is.
 */
void printConfig() {
  Serial.println("------------------------------------------------------");
  Serial.println("Configured SSID: " + ssid);
  Serial.println("Configured AP Password: " + APPassword);
  char Ip[15];
  IPAddress(APIP[0], APIP[1], APIP[2], APIP[3]).toString().toCharArray(Ip, 15);
  Serial.printf("Configured AP IP: %s\n", Ip);
  Serial.println("Configured APMode: " + APMode);
  Serial.printf("Configured channel: %s\n", channels[String(Channel).toInt()]);
  Serial.println("Configured STAMode: " + STAMode);
  Serial.println("Configured STA SSID: " + STASSID);
  Serial.println("Configured STAPassword: " + STAPassword);
  Serial.println("Configures STA Hostname: " + espHostname);
  Serial.println("Configured Auth: " + Auth);
  Serial.println("Configured AuthUsername: " + AuthUsername);
  Serial.println("Configured AuthPassword: " + AuthPassword);
  Serial.printf("Manual Sampling interval: %d\n", SIManual);
  Serial.println("Configured publishing policy: " + publishPolicy);
  Serial.println("Configured push URL: " + pushURL);
  Serial.println("Configured push Username: " + pushUsername);
  Serial.println("Configured push Password: " + pushPassword);

  Serial.println("Configured use NTP: " + UseNTP);
  Serial.printf("Configured NTP offset: %d\n", NTPOffset);
  Serial.println("Configured NTP pool URL: " + NTPPoolURL);
  Serial.println("------------------------------------------------------");  
}

/*
 * This function has the nodeMCU scan for available access point.
 * The access point are available for listing onced the scan
 * is finished.
 */
void scanAccessPoints() {
  
  Serial.println("Scanning for WiFi access points.");
  netCount = WiFi.scanNetworks();
  Serial.printf("%d networks available.\n", netCount);
  for(int i=0; i<netCount; i++) {
    Serial.printf("%d: %s, Ch:%d (%ddBm) %s\n", i + 1, WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i), WiFi.encryptionType(i) == ENC_TYPE_NONE ? "open" : "");
  }
    
}

/*
 * Setup the module on boot.
 */
void setup() {

  Serial.begin(115200);
  Serial.println();

  Serial.println("Allocating EEPROM memory for configuration parameters.");
  EEPROM.begin(ALLOCATED_EEPROM);

  scanAccessPoints();
  
  Serial.println("Loading configuration from EEPROM.");
  readConfig();
  printConfig();
  
  if(APMode.compareTo("true")==0) {
    Serial.println("Configuring access point...");
    
    //set-up the custom IP address
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(IPAddress(APIP[0], APIP[1], APIP[2], APIP[3]), IPAddress(APIP[0], APIP[1], APIP[2], APIP[3]), IPAddress(255, 255, 255, 0));   // subnet FF FF FF 00  
    
    char s[ssid.length()+1];
    ssid.toCharArray(s, sizeof(s));
    char p[APPassword.length()+1];
    APPassword.toCharArray(p, sizeof(p));
    WiFi.softAP(s, p, Channel, 0);
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    
  } else if(STAMode.compareTo("true")==0 && STASSID.length()!=0) {
    WiFi.mode(WIFI_STA);
    Serial.println("Connecting to '"+ STASSID + "' with password '" + STAPassword + "'");
    char h[espHostname.length()+1];
    espHostname.toCharArray(h, sizeof(h));
    char s[STASSID.length()+1];
    STASSID.toCharArray(s, sizeof(s));
    char p[STAPassword.length()+1];
    STAPassword.toCharArray(p, sizeof(p));
    WiFi.hostname(h);
    
    WiFi.begin(s, p);
    // Wait for connection
    int i = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      i++;
      if(i%30==0){
        Serial.println("");
      }
    }
    Serial.println("");
    Serial.println("Connected to: " + STASSID);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("Hostname: " + espHostname);

    if(UseNTP.compareTo("true")==0) {
      Serial.println("Setting up NTP support.");

      char u[NTPPoolURL.length()+1];
      NTPPoolURL.toCharArray(u, sizeof(u));
      //configTime(NTPOffset, 0, u);
      configTime(NTPOffset, 0, "nl.pool.ntp.org");
      Serial.println("\nWaiting for time");
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
  
  Serial.println("Attempting to initialize BME680");
  if (!bme.begin()) {
    Serial.println("Could not find a valid BME680 sensor, check BME680 address and/or check wiring!");
    while (1);
  }
  
  server.on ( "/", handleRoot );
  server.on ( "/sensor/read", handleSensorRead);
  server.on ( "/sensor/data", handleSensorData);
  server.on ("/settings", handleSettings);
  server.on ("/aplist", handleAPScan);
  server.on ("/nodemcu.css", handleCSS);
  server.on ("/nodemcu.js", handleJS);
  server.onNotFound ( handleNotFound );
  
  server.begin();
  Serial.println("HTTP server started");

}

/*
 * Main processing loop.
 */
void loop() {
  
  server.handleClient();
  // Get time.
  time_t now = time(nullptr);

  // Check if it is time to take a measurement.
  if(now%SIManual==0 && !measurementTaken) {
    measurementTaken = true;
    if (! bme.performReading()) {
      Serial.println("Failed to perform reading");
      measurementTaken = false;
    } else {
      lastMeasurement = time(nullptr);
      Serial.println(ctime(&lastMeasurement));
      Serial.print(PSTR("Temperature = ")); Serial.print(bme.temperature); Serial.println(" *C");
      Serial.print("Pressure = "); Serial.print(bme.pressure / 100.0); Serial.println(" hPa");
      Serial.print("Humidity = "); Serial.print(bme.humidity); Serial.println(" %");
      Serial.print("Gas = "); Serial.print(bme.gas_resistance / 1000.0); Serial.println(" KOhms");

      if(publishPolicy.compareTo("Push")==0) {
        HTTPClient http;    //Declare object of class HTTPClient
        
        http.begin(pushURL);      //Specify request destination
        http.addHeader("Content-Type", "application/json");  //Specify content-type header
        
        snprintf(html, 500, SENSOR_HTML, lastMeasurement, bme.temperature, bme.humidity, bme.pressure / 100.0, bme.gas_resistance, 0, "Success");
        Serial.println(html);
        int httpCode = http.POST(html);     //Send the request
        String payload = http.getString();  //Get the response payload
        
        Serial.println(httpCode);   //Print HTTP return code
        Serial.println(payload);    //Print request response payload
        
        http.end();  //Close connection
      }


      
    }
  } else {
    measurementTaken = false;
  }
  
}
