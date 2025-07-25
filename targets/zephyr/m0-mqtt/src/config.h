#ifndef __CONFIG_H__
#define __CONFIG_H__

#define ZEPHYR_ADDR		"192.168.1.101"
#define SERVER_ADDR		"192.168.8.102"
#define SERVER_PORT		1883
#define APP_CONNECT_TIMEOUT_MS	2000
#define APP_SLEEP_MSECS		200
#define APP_CONNECT_TRIES	5
#define APP_MQTT_BUFFER_SIZE	128
#define MQTT_CLIENTID		"zephyr_publisher"

#endif
