#ifndef CONFIG_H
#define CONFIG_H

#include <WiFi.h>

struct broker_details
{
    const char *mf_thing_id = "845c3b52-583b-45d2-be14-f46a1f7f5d3f";
    const char *mf_thing_pass = "123456789";
    const char *mf_channel_id = "1eab858c-7407-4c3e-97b2-f357ee95a5b8";
    const char *mf_topic = "channels/1eab858c-7407-4c3e-97b2-f357ee95a5b8/messages";
} br_config;

struct wifi_details
{
    const char *wifi_ssid = "OH_LAN";
    const char *wifi_pass = "0799973955";
} wifi_config;

IPAddress server(192,168,0,130);

#endif