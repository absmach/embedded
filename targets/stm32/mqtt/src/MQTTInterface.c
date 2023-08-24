#include <string.h>

#include "lwip.h"
#include "lwip/api.h"
#include "lwip/sockets.h"
#include "stm32f4xx_hal.h"
#include "MQTTInterface.h"


#define MQTT_PORT	1883

uint32_t MilliTimer;

char timerIsExpired(Timer *timer) {
	long left = timer->end_time - MilliTimer;
	return (left < 0);
}

void timerCountDownMS(Timer *timer, unsigned int timeout) {
	timer->end_time = MilliTimer + timeout;
}

void timerCountDown(Timer *timer, unsigned int timeout) {
	timer->end_time = MilliTimer + (timeout * 1000);
}

int timerLeftMS(Timer *timer) {
	long left = timer->end_time - MilliTimer;
	return (left < 0) ? 0 : left;
}

void initTimer(Timer *timer) {
	timer->end_time = 0;
}

void newNetwork(Network *n) {
	n->conn = NULL;
	n->buf = NULL;
	n->offset = 0;
	n->mqttread = netRead;
	n->mqttwrite = netWrite;
	n->disconnect = netDisconnect;
}

int connectNetwork(Network *n, char *ip, int port) {
	err_t err;

	n->conn = netconn_new(NETCONN_TCP);
	if (n->conn != NULL) {
		err = netconn_connect(n->conn, &ip, port);

		if (err != ERR_OK) {
			netconn_delete(n->conn);
			return -1;
		}
	}

	return 0;
}

int netRead(Network *n, unsigned char *buffer, int len, int timeout_ms) {
	int rc;
	struct netbuf *inbuf;
	int offset = 0;
	int bytes = 0;

	while(bytes < len) {
		if(n->buf != NULL) {
			inbuf = n->buf;
			offset = n->offset;
			rc = ERR_OK;
		} else {
			rc = netconn_recv(n->conn, &inbuf);
			offset = 0;
		}

		if(rc != ERR_OK) {
			if(rc != ERR_TIMEOUT) {
				bytes = -1;
			}
			break;
		} else {
			int nblen = netbuf_len(inbuf) - offset;
			if((bytes+nblen) > len) {
				netbuf_copy_partial(inbuf, buffer+bytes, len-bytes,offset);
				n->buf = inbuf;
				n->offset = offset + len - bytes;
				bytes = len;
			} else {
				netbuf_copy_partial(inbuf, buffer+bytes, nblen, offset);
				bytes += nblen;
				netbuf_delete(inbuf);
				n->buf = NULL;
				n->offset = 0;
			}
		}
	}
	return bytes;
}

int netWrite(Network *n, unsigned char *buffer, int len, int timeout_ms) {
	int rc = netconn_write(n->conn, buffer, len, NETCONN_NOCOPY);
	if(rc != ERR_OK) return -1;
	return len;
}

void netDisconnect(Network *n) {
	netconn_close(n->conn);
	netconn_delete(n->conn); 
	n->conn = NULL;
}