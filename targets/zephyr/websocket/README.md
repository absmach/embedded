# Magistrala WebSocket Client for Zephyr

This is a Zephyr RTOS application that connects to a Magistrala IoT platform using WebSocket protocol and sends telemetry data.

## Features

- WiFi connectivity with automatic IP address acquisition via DHCP
- WebSocket client with text frame support
- Persistent bidirectional communication
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
#define MAGISTRALA_WS_PORT 8186
#define DOMAIN_ID "your-domain-id"
#define CLIENT_ID "your-client-id"
#define CLIENT_SECRET "your-client-secret"
#define CHANNEL_ID "your-channel-id"
```

### Application Configuration

```c
#define TELEMETRY_INTERVAL_SEC 30  // Telemetry send interval in seconds
```

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

## WebSocket Connection Structure

The client connects to:

```text
ws://{MAGISTRALA_HOSTNAME}:{MAGISTRALA_WS_PORT}/m/{DOMAIN_ID}/c/{CHANNEL_ID}?authorization={CLIENT_SECRET}
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

The client uses WebSocket URI query parameter authentication:

- Query parameter: `authorization={CLIENT_SECRET}`

## WebSocket Features

- WebSocket protocol upgrade from HTTP
- Text data frames (WEBSOCKET_OPCODE_DATA_TEXT)
- Final frame flag support
- Persistent connection for continuous data transmission
- Custom headers support (Origin header)
- Connection callback for status monitoring

## Error Handling

- Automatic system reboot on WiFi connection failure
- Automatic system reboot on DHCP timeout
- Socket creation and connection error handling
- WebSocket handshake error handling
- Send error detection with connection break

## Protocol Details

- Transport: TCP (SOCK_STREAM)
- WebSocket upgrade via HTTP/1.1
- Frame masking (required for client-to-server)
- Text data frames
- Continuous connection mode
- Temporary buffer for WebSocket framing (512 + 30 bytes)

## Connection Management

- Single persistent WebSocket connection
- Continuous telemetry transmission
- Graceful connection closure on error
- Automatic resource cleanup (socket closure)
