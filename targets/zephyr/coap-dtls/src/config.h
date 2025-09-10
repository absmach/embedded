#ifndef CONFIG_H
#define CONFIG_H

/* STA Mode Configuration */
#define WIFI_SSID "SSID"    // Replace `SSID` with WiFi ssid
#define WIFI_PSK "PASSWORD" // Replace `PASSWORD` with Router password

/* Magistrala Configuration */
#define MAGISTRALA_IP                                                          \
  "192.168.8.126" // Replace with your Magistrala instance IP
#define MAGISTRALA_COAP_PORT 5688  // DTLS port for CoAPS (CoAP over DTLS)
#define DOMAIN_ID "37e191d5-0e57-4d27-b384-26f3c1439561"         // Replace with your Domain ID
#define CLIENT_ID "c658801e-ef14-4aca-a024-7fa034e95624"         // Replace with your Client ID
#define CLIENT_SECRET "316377fe-a105-4afa-85fb-ca020255c5dc" // Replace with your Client secret
#define CHANNEL_ID "6f81fcb4-0751-414e-a1ce-912045d0a1c7"       // Replace with your Channel ID
#define MQTT_CLIENTID "c658801e-ef14-4aca-a024-7fa034e95624" // Replace with your actual client ID

#endif