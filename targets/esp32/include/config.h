#ifndef CONFIG_H
#define CONFIG_H

#include <WiFi.h>

struct broker_details
{
    const char *mf_thing_id = "";
    const char *mf_thing_pass = "";
    const char *mf_channel_id = "";
    const char *mf_topic = "";
} br_config;

struct wifi_details
{
    const char *wifi_ssid = "";
    const char *wifi_pass = "";
} wifi_config;

IPAddress server(000,000,000,000);

#endif
