# Magistrala HTTPS Client for Zephyr

This is a Zephyr RTOS application that connects to a Magistrala IoT platform using HTTPS protocol (HTTP over TLS) and sends telemetry data.

## Features

- WiFi connectivity with automatic IP address acquisition via DHCP
- Secure HTTPS client with TLS/SSL encryption
- HTTP POST support over TLS
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
#define MAGISTRALA_HTTPS_PORT 443
#define DOMAIN_ID "your-domain-id"
#define CLIENT_ID "your-client-id"
#define CLIENT_SECRET "your-client-secret"
#define CHANNEL_ID "your-channel-id"
```

### Application Configuration

```c
#define TELEMETRY_INTERVAL_SEC 30  // Telemetry send interval in seconds
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

## HTTPS Endpoint Structure

The client sends POST requests to:

```text
/api/http/m/{DOMAIN_ID}/c/{CHANNEL_ID}
```

## Payload Format

The client sends data in SenML JSON format:

```json
[
  {
    "bn": "esp32s3:",
    "bt": 1761700000,
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

The client uses HTTP header-based authentication over TLS:

- Header: `Authorization: Client {CLIENT_SECRET}`
- Content-Type: `application/senml+json`
- TLS: Server certificate verification with CA certificate

## Security

- TLS 1.2 encryption
- Server certificate verification (TLS_PEER_VERIFY_REQUIRED)
- CA certificate validation
- SNI (Server Name Indication) support

## Error Handling

- Automatic system reboot on WiFi connection failure
- Automatic system reboot on DHCP timeout
- Connection timeout handling (3 seconds)
- Socket creation and connection error handling
- TLS handshake error handling

## HTTPS Features

- HTTP/1.1 protocol over TLS
- POST method for telemetry data
- Content-Type and Authorization headers
- Response callback for status checking
- Secure socket configuration with TLS options
