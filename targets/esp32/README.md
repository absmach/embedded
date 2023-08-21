# MQTTS- esp32 target
## Requirements
1. Mainflux broker details including: hostname, ThingID, Thing Credentials and Channel ID
2. [PlatformIO](https://platformio.org/)
3. [esp tool](https://github.com/espressif/esptool)

## Configure
1. Edit the platform.ini file for the specific target.
2. Edit the [config file](include/config.h) with your broker and network details.
3. Add a valid certificate to the certificate.h folder.

## Build
The project can be built by utilising the make file within the target directory

```bash
make 
```
## Flash
Platform io generate a build directory with the fimware.bin within it. Use the esp tool to flash the bin to the connected board.
```bash
cd  .pio/build/<target>
esptool.py --port <port> write_flash <address> firmware.bin
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