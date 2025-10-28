#ifndef CONFIG_H
#define CONFIG_H

/* STA Mode Configuration */
#define WIFI_SSID "internal"    // Replace `SSID` with WiFi ssid
#define WIFI_PSK "$$Osiepna2024" // Replace `PASSWORD` with Router password

/* Mosquitto Test Broker Configuration */
#define MOSQUITTO_BROKER_HOSTNAME "messaging.magistrala-beta.absmach.eu"
#define MOSQUITTO_BROKER_PORT 8883  // Encrypted, unauthenticated MQTTS port
#define MQTT_CLIENTID "zephyr_mqtts_client_new"  // Replace with unique client ID
#define CLIENT_SECRET "f6883013-af94-44c4-bdef-7188089c05fd" // Replace with your Client secret
#define CLIENT_ID "2fde3a36-e098-485c-8b83-314facaef903"         // Replace with your Client ID

/* MQTT Topic Configuration */
#define MQTT_PUBLISH_TOPIC "m/49baf6fe-2f7c-4748-8e99-2846346ef6ba/c/0ffc71bc-4a25-4e22-bfb8-b1a847ed50ab/pub"
#define MQTT_SUBSCRIBE_TOPIC "m/49baf6fe-2f7c-4748-8e99-2846346ef6ba/c/0ffc71bc-4a25-4e22-bfb8-b1a847ed50ab/sub"

/* Application Configuration */
#define APP_MQTT_BUFFER_SIZE 1024
#define APP_CONNECT_TRIES 3
#define APP_CONNECT_TIMEOUT_MS 5000
#define APP_SLEEP_MSECS 1000
#define TELEMETRY_INTERVAL_SEC 30

#endif
