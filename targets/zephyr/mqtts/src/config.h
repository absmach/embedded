#ifndef CONFIG_H
#define CONFIG_H

/* STA Mode Configuration */
#define WIFI_SSID "SSID"    // Replace `SSID` with WiFi ssid
#define WIFI_PSK "PASSWORD" // Replace `PASSWORD` with Router password

/* Magistrala Configuration */
#define MAGISTRALA_HOSTNAME "MAGISTRALA_HOSTNAME" // Replace with your Magistrala instance hostname or IP
#define MAGISTRALA_MQTTS_PORT 8883
#define DOMAIN_ID "DOMAIN_ID"         // Replace with your Domain ID
#define CLIENT_ID "CLIENT_ID"         // Replace with your Client ID
#define CLIENT_SECRET "CLIENT_SECRET" // Replace with your Client secret
#define CHANNEL_ID "CHANNEL_ID"       // Replace with your Channel ID

/* Application Configuration */
#define APP_MQTT_BUFFER_SIZE 1024
#define APP_CONNECT_TRIES 3
#define APP_CONNECT_TIMEOUT_MS 5000
#define APP_SLEEP_MSECS 1000
#define TELEMETRY_INTERVAL_SEC 30
#define MAX_ITERATIONS 10
#define MAX_CONNECTIONS 1

#endif
