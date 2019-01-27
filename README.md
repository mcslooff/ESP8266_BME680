# ESP8266_BME680
NodeMCU (ESP8266) with BME680 publishing measurements with JSON

This project aims to build a fully configurable flash for the ESP8266 NodeMCU with a BME 680 on the I2C bus.
Configuration can be done either via a web-page or via submitting JSON.
Initially the module starts in Access Point mode after which the configuration page is reachable at the default
IP address 168.192.1.4 Then mode and IP can be changed. The module can be set boot in station mode connecting to
a WiFi access point of your choice.
The module can be set to either push the measurements to a remote server or wait for a client to poll the measurements from it.

To access the node a username and password are required.
The default Access Point password is: ESP8266Test
The default username and password to access the configuration page is: admin and admin.

TODO:
-Add TLS
-Add security on the push mechanism
-Add mDNS
-Add download and upload of configuration (for back-up purposes)
-Add restore to factory default by pressing the reset button 3 times within 60 seconds.
