# MQTTS- stm32 target
## Requirements
1. Mainflux broker details including: hostname, ThingID, Thing Credentials and Channel ID
2. [PlatformIO](https://platformio.org/)
3. [dfu-util](https://dfu-util.sourceforge.net/)

## Configure
1. Edit the platform.ini file for the specific target.
2. Edit the [config file](include/config.h) with your broker and network details.

## Build
The project can be built by utilising the make file within the target directory

```bash
make 
```
## Flash
Platform io generate a build directory with the fimware.bin within it. Use the make command to flash to board
```bash
make upload
```
## Log
The program logs to the serial monitor. On successful connection should show:
```bash
Connected to WiFi
IP: 
Connecting to MQTT broker
Connected to MQTT broker
Publish success
```