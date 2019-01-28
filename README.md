# ESP8266 NodeMCU with Bosch BME680

This project aims to provide a fully configurable module for the ESP8266 with a BME680 on the I2C port. The general purpose is to provide a
flash to be loaded on the ESP8266 with factory defaults and provide the end-user with the means to configure it to her needs. The module is
intended to provide the following features: factory default configuration, push or pull data from the sensor, basic authentication, 
NTP synchronisation, Multicast DNS, TLS v1.2, MQTT option. Data is published as JSON to a web-server or can be pulled from the module or one
can configure the MQTT to push it to a queue.

## Getting Started

Clone the source code and ensure to have the following libraries installed: list of libraries to follow.

### Prerequisites

To be able to successfully run this code you need the Adafruit Feather HUZZAH with ESP8266 WiFi and the BME680 Breakout - Air Quality, Temperature, Pressure, Humidity Sensor.
I bought both modules from Kiwi Electronics in Rijswijk from the pimoroni.com manufacturer.

To get started first install the Arduino development software from https://www.arduino.cc/en/Main/Software
Next install the following boards and libraries.

```
Give examples
```

Clone or download the .ino-file and open it in your Arduino IDE. Press compile to see if it all works. Then if it does flash your ESP8266 with it.

### Hardware requirements and set-up

As mentioned you need the Adafruit Feather HUZZAH with ESP8266 WiFi and the BME680 Breakout - Air Quality, Temperature, Pressure, Humidity Sensor.
The BME680 comes in various forms and with various addresses pre-configured. There are two addresses in use 0x76 and 0x77. You can either change
the breakout board by bridging or breaking the jumper for the address, or change the software to pick the right address. I opted for the latter and
changed the BME680.h file to pick the right address.
Hookup the BME680 to the I2C of the ESP8266 and your ready. There's plenty of documentation to find on how to do that.

### Features

The main feature of this project is to create a configuration interface for the boards that allows a user, that is not a software developer or
electronics hobyist to use the module and configure it properly. This ESP8266-BME680 project is just the first part of it and will be followed by
server software for data collection and processing on the Raspberry Pi or any other platform supporting Java.

### Installing

A step by step series of examples that tell you how to get a development env running

Say what the step will be

```
Give the example
```

And repeat

```
until finished
```

End with an example of getting some data out of the system or using it for a little demo

## Running the tests

Explain how to run the automated tests for this system

### Break down into end to end tests

Explain what these tests test and why

```
Give an example
```

### And coding style tests

Explain what these tests test and why

```
Give an example
```

## Deployment

Add additional notes about how to deploy this on a live system

## Built With

* [Arduino IDE](https://www.arduino.cc/en/Main/Software) - The main development tool for prototyping.

## Contributing

Please read [CONTRIBUTING.md](https://gist.github.com/PurpleBooth/b24679402957c63ec426) for details on our code of conduct, and the process for submitting pull requests to us.

## Versioning

...

## Authors

* **Marcel Slooff** - *Initial work* - [mcslooff](https://github.com/mcslooff)

## License

This project is licensed under the GNU License - see the [LICENSE.md](LICENSE.md) file for details

## Acknowledgments

* Hat tip to anyone whose code was used
* Inspiration
* etc
