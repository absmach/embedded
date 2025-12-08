#ifndef WIFI_H
#define WIFI_H

#include <zephyr/net/net_if.h>

void initialize_wifi(void);
int connect_to_wifi(struct net_if *sta_iface, char *ssid, char *psk);

#endif
