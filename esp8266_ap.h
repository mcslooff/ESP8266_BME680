#define SEALEVELPRESSURE_HPA (1013.25)
#define SI_COUNT 12
#define CH_COUNT 14
#define HTML_PAGE_BUFFER_SIZE 12000

const byte NON_FACTORY = 15;
const int ALLOCATED_EEPROM = 1024;

struct _settings {
  int serverPort;
  char accessPointSSID[20];
  byte accessPointIP[4];
  boolean accessPointMode;
  char accessPointPassword[20];
  byte accessPointChannel;
  boolean stationMode;
  char stationSSID[50];
  char stationAccessPointPassword[20];
  boolean stationRequireAuthentication;
  char stationUsername[20];
  char stationPassword[20];
  int sensorSampleInterval;
  char publishingPolicy[6];
  char publishingURL[100];
  char publishingUsername[20];
  char publishingPassword[20];
  boolean useNTP;
  char NTPPoolURL[50];
  int NTPOffset;
  char hostName[20];
};

/* 
 * These are the factory default values
 * We'd prefer to have them in PROGMEM
 * but if we do the application crashes.
*/
_settings factoryDefaults = {
  80,
  "ESP8266",
  {192, 168, 4, 1},
  true,
  "ESP8266Test",
  0,
  false,
  "",
  "",
  true,
  "admin",
  "admin",
  10,
  "Poll",
  "",
  "",
  "",
  true,
  "nl.pool.ntp.org",
  3600,
  "NodeMCU"
  };

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
  " \"NTPPoolURL\": \"%s\",\n"
  " \"serverPort\": %d\n"
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
  "var x = setInterval(function() {loadData(\"/status\", \"status\", updateData)}, 10000);\n"
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
  "     <button class=\"tablinks\" onclick=\"openConfigSheet(event, 'Server')\">Web server</button>\n"
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
  "     <div id=\"Server\" class=\"tabcontent\">\n"
  "       <h3>Web server</h3>\n"
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
  "       <tr>\n"
  "         <td>Port:</td>\n"
  "         <td>\n"
  "           <input id=\"serverPort\" name=\"serverPort\" type=\"text\">\n"
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

char STATUS_HTML[] PROGMEM =
  "<table>\n"
  " <tr>\n"
  "   <td>Up-time:</td>\n"
  "   <td>%d days %d hours %d minutes %d seconds</td>\n"
  " <tr>\n"
  " <tr>\n"
  "   <td>System time:</td>\n"
  "   <td>%s</td>\n"
  " <tr>\n"
  " <tr>\n"
  "   <td>Last measurment:</td>\n"
  "   <td>%s</td>\n"
  " <tr>\n"
  " <tr>\n"
  "   <td>Node name:</td>\n"
  "   <td>%s</td>\n"
  " <tr>\n"
  " <tr>\n"
  "   <td>IP address:</td>\n"
  "   <td>%s</td>\n"
  " <tr>\n"
  " <tr>\n"
  "   <td>Temperature:</td>\n"
  "   <td>%f &#176;C</td>\n"
  " <tr>\n"
  " <tr>\n"
  "   <td>Humity</td>\n"
  "   <td>%f &#37;</td>\n"
  " <tr>\n"
  " <tr>\n"
  "   <td>Air pressure:</td>\n"
  "   <td>%f hPa</td>\n"
  " <tr>\n"
  " <tr>\n"
  "   <td>VOC</td>\n"
  "   <td>%f k&#937;</td>\n"
  " <tr>\n"
  "</table>\n";

const char SENSOR_HTML[] PROGMEM = "{\"stationName\":\"%s\", \"timestamp\":%d, \"temperature\":%f, \"humidity\":%f, \"air-pressure\":%f, \"voc\":%f, \"resultCode\": %d, \"resultText\", \"%s\"}";

// Declare String to manipulate html data.
char html[HTML_PAGE_BUFFER_SIZE];
