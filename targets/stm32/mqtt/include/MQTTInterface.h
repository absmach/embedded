#ifndef _MQTTInterface_H
#define _MQTTInterface_H

typedef struct Timer Timer;

struct Timer
{
	unsigned long systick_period;
	unsigned long end_time;
};

typedef struct Network Network;

struct Network
{
	struct netconn *conn;
	struct netbuf *buf;
	int offset;
	int (*mqttread)(Network *, unsigned char *, int, int);
	int (*mqttwrite)(Network *, unsigned char *, int, int);
	void (*disconnect)(Network *);
};

void initTimer(Timer *);
char timerIsExpired(Timer *);
void timerCountDownMS(Timer *, unsigned int);
void timerCountDown(Timer *, unsigned int);
int timerLeftMS(Timer *);

int netRead(Network *, unsigned char *, int, int);
int netWrite(Network *, unsigned char *, int, int);
void netDisconnect(Network *);
void newNetwork(Network *);
int connectNetwork(Network *, char *, int);

#endif
