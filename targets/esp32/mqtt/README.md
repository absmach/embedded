# MQTTS- esp32 target
## Requirements
1. Mainflux broker details including: hostname, ThingID, Thing Credentials and Channel ID
2. [PlatformIO](https://platformio.org/)


## Configure
1. Edit the [config file](include/config.h) with your broker details.
2. Update the [cnetwork.c](src/cnetwork.c) with your network details.
3. COnfigure the platform io file for your specific target.

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
