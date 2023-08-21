#ifndef CONFIG_H
#define CONFIG_H

#include <WiFiClientSecure.h>

struct BrokerDetails
{
    const char *mfThingId = "64c2fc54-7d5c-4b4f-903c-e628ff7aa23e";
    const char *mfThingPass = "24eb50a8-f281-4789-9435-1d4909086da4";
    const char *mfChannelId = "1eab858c-7407-4c3e-97b2-f357ee95a5b8";
    char mfTopic[150];
    BrokerDetails()
    {
        const char *_preId = "channels/";
        const char *_postId = "/messages";
        strcat(mfTopic, _preId);
        strcat(mfTopic, mfChannelId);
        strcat(mfTopic, _postId);
    }
} brConfig;

struct WifiDetails
{
    const char *wifiSsid = "Octavifi";
    const char *wifiPass = "Unic0rn_2030";
} wifiConfig;

// IPAddress server(192, 168, 100, 122);

const char *server = "felix-Aspire-AV15-51.local";

#endif
