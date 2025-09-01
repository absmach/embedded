#include "config.h"
#include <zephyr/logging/log.h>
#include <zephyr/net/wifi_mgmt.h>

LOG_MODULE_DECLARE(coap_client);

struct wifi_connect_req_params sta_config;

static struct net_mgmt_event_callback cb;

#define NET_EVENT_WIFI_MASK                                                    \
  (NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT |          \
   NET_EVENT_WIFI_AP_ENABLE_RESULT | NET_EVENT_WIFI_AP_DISABLE_RESULT)

void wifi_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
                        struct net_if *iface) {
  switch (mgmt_event) {
  case NET_EVENT_WIFI_CONNECT_RESULT: {
    LOG_INF("Connected to %s", WIFI_SSID);
    break;
  }
  case NET_EVENT_WIFI_DISCONNECT_RESULT: {
    LOG_INF("Disconnected from %s", WIFI_SSID);
    break;
  }
  case NET_EVENT_WIFI_AP_ENABLE_RESULT: {
    LOG_INF("AP Mode is enabled. Waiting for station to connect");
    break;
  }
  case NET_EVENT_WIFI_AP_DISABLE_RESULT: {
    LOG_INF("AP Mode is disabled.");
    break;
  }
  default:
    break;
  }
}

void initialize_wifi(void) {
  net_mgmt_init_event_callback(&cb, wifi_event_handler, NET_EVENT_WIFI_MASK);
  net_mgmt_add_event_callback(&cb);
}

int connect_to_wifi(struct net_if *sta_iface, char *ssid, char *psk) {
  if (!sta_iface) {
    LOG_INF("STA: interface no initialized");
    return -EIO;
  }

  sta_config.ssid = (const uint8_t *)ssid;
  sta_config.ssid_length = strlen(ssid);
  sta_config.psk = (const uint8_t *)psk;
  sta_config.psk_length = strlen(psk);
  sta_config.security = WIFI_SECURITY_TYPE_PSK;
  sta_config.channel = WIFI_CHANNEL_ANY;
  sta_config.band = WIFI_FREQ_BAND_2_4_GHZ;

  LOG_INF("Connecting to SSID: %s\n", sta_config.ssid);

  int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, sta_iface, &sta_config,
                     sizeof(struct wifi_connect_req_params));
  if (ret) {
    LOG_ERR("Unable to Connect to (%s)", ssid);
  }

  return ret;
}
