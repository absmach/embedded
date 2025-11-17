# Magistrala CoAPS Client for Zephyr

This is a Zephyr RTOS application that connects to a Magistrala IoT platform using CoAPS (CoAP over DTLS) and sends telemetry data.

## Features

- WiFi connectivity with automatic IP address acquisition via DHCP
- Secure CoAP client with DTLS encryption
- CoAP with confirmable (CON) message support
- UDP-based communication with DTLS security
- Certificate-based server authentication
- Query parameter-based authentication
- Simulated sensor data (temperature, humidity, battery level, LED state)
- SenML JSON payload format
- Automatic error handling
- Configurable telemetry interval

## Hardware Requirements

- ESP32-S3 or compatible board with WiFi support
- Zephyr RTOS support

## Requirements

1. Magistrala broker details including: hostname, Domain ID, Client ID, Client Secret and Channel ID
2. [Zephyr](https://www.zephyrproject.org/)
3. CA certificate for DTLS verification (provided in `src/ca_cert.h`)

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
#define MAGISTRALA_COAPS_PORT 5684
#define DOMAIN_ID "your-domain-id"
#define CLIENT_ID "your-client-id"
#define CLIENT_SECRET "your-client-secret"
#define CHANNEL_ID "your-channel-id"
```

### Application Configuration

```c
#define TELEMETRY_INTERVAL_SEC 30  // Telemetry send interval in seconds
```

### DTLS Certificate

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

## CoAPS Resource Structure

The client sends POST requests to:

```text
coaps://{MAGISTRALA_HOSTNAME}:{MAGISTRALA_COAPS_PORT}/m/{DOMAIN_ID}/c/{CHANNEL_ID}?auth={CLIENT_SECRET}
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

The client uses CoAP URI query parameter authentication over DTLS:

- Query parameter: `auth={CLIENT_SECRET}`
- DTLS: Server certificate verification with CA certificate

## Security

- DTLS 1.0/1.2 encryption
- Server certificate verification (TLS_PEER_VERIFY_REQUIRED)
- CA certificate validation
- SNI (Server Name Indication) support

## CoAPS Features

- CoAP Version 1 over DTLS
- Confirmable (CON) message type
- POST method for telemetry data
- Token-based message correlation
- Message ID for deduplication
- Response timeout handling (5 seconds)
- DTLS handshake

## Error Handling

- Automatic system reboot on WiFi connection failure
- Automatic system reboot on DHCP timeout
- Socket creation error handling
- DTLS handshake error handling
- CoAP packet initialization and parsing error handling
- Response timeout handling

## Protocol Details

- Transport: UDP with DTLS (SOCK_DGRAM + IPPROTO_DTLS_1_0)
- Default port: 5684
- Message format: CoAP binary over DTLS
- Payload marker support
- Security tag for DTLS credentials
