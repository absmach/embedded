#ifndef WIFI_H
#define WIFI_H

#include <zephyr/net/wifi_mgmt.h>

int connect_to_wifi(struct net_if *sta_iface, char *ssid, char *psk);
void initialize_wifi(void);

#endif
