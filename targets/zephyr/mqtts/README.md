# Magistrala MQTTS Client for Zephyr

This is a Zephyr RTOS application that connects to a Magistrala IoT platform using MQTT over TLS (MQTTS) protocol and sends telemetry data.

## Features

- WiFi connectivity with automatic IP address acquisition via DHCP
- Secure MQTT client with TLS/SSL encryption
- MQTT with QoS 0, 1, and 2 support
- MQTT subscribe and publish capabilities
- Certificate-based server authentication
- Simulated sensor data (temperature, humidity, battery level, LED state)
- SenML JSON payload format
- Automatic reconnection and error handling
- Configurable telemetry interval

## Hardware Requirements

- ESP32-S3 or compatible board with WiFi support
- Zephyr RTOS support

## Requirements

1. Magistrala broker details including: hostname, Domain ID, Client ID, Client Secret and Channel ID
2. [Zephyr](https://www.zephyrproject.org/)
3. CA certificate for TLS verification (provided in `src/ca_cert.h`)

## Configuration

Edit `src/config.h` to configure your settings:

### WiFi Configuration

```c
#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PSK "your-wifi-password"
```

### Magistrala Configuration

```c
#define MAGISTRALA_HOSTNAME "your-magistrala-instance.com"
#define MAGISTRALA_MQTTS_PORT 8883
#define DOMAIN_ID "your-domain-id"
#define CLIENT_ID "your-client-id"
#define CLIENT_SECRET "your-client-secret"
#define CHANNEL_ID "your-channel-id"
```

### Application Configuration

```c
#define APP_MQTT_BUFFER_SIZE 1024
#define APP_CONNECT_TRIES 3
#define APP_CONNECT_TIMEOUT_MS 5000
#define APP_SLEEP_MSECS 1000
#define TELEMETRY_INTERVAL_SEC 30
```

### TLS Certificate

Update `src/ca_cert.h` with your Magistrala server's CA certificate.

## Build

The project can be built by utilizing west within the target directory:

```bash
west build -p always -b esp32s3_devkitc/esp32s3/procpu
```

## Flash

Use the west command to flash to board:

```bash
west flash
```

## Monitoring

Monitor serial output:

```bash
west espressif monitor
```

## MQTT Topic Structure

The client subscribes and publishes to the following topic:

```text
m/{DOMAIN_ID}/c/{CHANNEL_ID}
```

## Payload Format

The client sends data in SenML JSON format:

```json
[
  {
    "bn": "esp32s3:",
    "bt": 1761835000,
    "bu": "Cel",
    "bver": 5,
    "n": "temperature",
    "u": "Cel",
    "v": 23.45
  },
  {
    "n": "humidity",
    "u": "%RH",
    "v": 65.30
  },
  {
    "n": "battery",
    "u": "%",
    "v": 85
  },
  {
    "n": "led",
    "vb": false
  }
]
```

## Authentication

The client uses MQTT username/password authentication over TLS:

- Username: `CLIENT_ID`
- Password: `CLIENT_SECRET`
- TLS: Server certificate verification with CA certificate

## Security

- TLS 1.2 encryption
- Server certificate verification (TLS_PEER_VERIFY_REQUIRED)
- CA certificate validation
- SNI (Server Name Indication) support

## QoS Levels

The client demonstrates all three MQTT QoS levels:

- QoS 0: At most once delivery
- QoS 1: At least once delivery
- QoS 2: Exactly once delivery

## Error Handling

- Automatic system reboot on WiFi connection failure
- Automatic system reboot on DHCP timeout
- Connection retry mechanism with configurable attempts
- MQTT keepalive and ping support
- TLS handshake error handling
