MQTTS- stm32f4 target

## Requirements

1. Mainflux broker details including: hostname, ThingID, Thing Credentials and Channel ID
2. [Zephyr](https://www.zephyrproject.org/)
3. [OpenSSL](https://www.openssl.org/)

## Configure

1. Edit the [config file](include/config.h) with your broker details.

2. Generate Root CA

   ```bash
   cd targets/zephyr_targets/mqtts/src/creds
   openssl genrsa -out magistralaRootCA.key 4096
   openssl req -x509 -new -nodes -days 3650 -key magistralaRootCA.key -out magistralaRootCA.pem
   ```

3. Generate Device Certificate

   ```bash
   cd targets/zephyr_targets/mqtts/src/creds
   openssl genrsa -out device.key 2048
   openssl req -new -key device.key -out device.csr
   openssl x509 -req -in device.csr -CA magistralaRootCA.pem -CAkey magistralaRootCA.key -CAcreateserial -out device.crt -days 365 -sha256
   mv device.crt device-certificate.pem.crt
   mv device.key device-private.pem.key
   ```

4. Convert the Keys to code

   ```bash
   cd targets/zephyr_targets/mqtts/src/creds
   python3 convert_keys.py
   ```

## Build

The project can be built by utilising the make file within the target directory

```bash
west build -p always -b <your-board-name> mqtt
```

## Flash

Platform io generate a build directory with the fimware.bin within it. Use the make command to flash to board

```bash
west flash
```
