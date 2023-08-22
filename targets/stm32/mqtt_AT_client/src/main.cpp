#include <Arduino.h>
#include <WiFiEsp.h>
#include <PubSubClient.h>

#include "config.h"

#define LOG_PRINTFLN(fmt, ...) logfln(fmt, ##__VA_ARGS__)
#define LOG_SIZE_MAX 128
#define HW_UART_SPEED 115200L
#define MQTT_ID "STM32"

WiFiEspClient espClient;
PubSubClient client(espClient);
int status = WL_IDLE_STATUS;
HardwareSerial Serial2(USART2);
char msg[50];

void logfln(const char *fmt, ...)
{
	char buf[LOG_SIZE_MAX];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, LOG_SIZE_MAX, fmt, ap);
	va_end(ap);
	Serial.println(buf);
}

void setup_wifi()
{

	// initialize serial for ESP module
	Serial2.begin(HW_UART_SPEED);
	// initialize ESP module
	WiFi.init(&Serial2);
	delay(10);
	// We start by connecting to a WiFi network
	LOG_PRINTFLN(" ");
	LOG_PRINTFLN("Connecting to ");
	LOG_PRINTFLN(wifiConfig.wifiSsid);

	WiFi.begin(wifiConfig.wifiSsid, wifiConfig.wifiPass);

	while (WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		Serial.print(".");
	}

	LOG_PRINTFLN("");
	LOG_PRINTFLN("Connected to WiFi");
}

void processMessage(char *topic, byte *payload, unsigned int length)
{
	LOG_PRINTFLN("Message arrived [");
	LOG_PRINTFLN(topic);
	LOG_PRINTFLN("] ");
	for (int i = 0; i < length; i++)
	{
		LOG_PRINTFLN((const char *)payload[i]);
	}
	LOG_PRINTFLN(" ");
}
void setup()
{
	Serial.begin(HW_UART_SPEED);
	setup_wifi();
	client.setServer(server, 1883);
	client.setCallback(processMessage);
}

void loop()
{
	if (!client.connected())
	{
		LOG_PRINTFLN("Connecting to MQTT broker");
		// Attempt to connect
		if (client.connect(MQTT_ID, brConfig.mfThingId, brConfig.mfThingPass))
		{
			LOG_PRINTFLN("connected to MQTT broker");
			// Subscribe
			client.subscribe(brConfig.mfTopic);
		}
		else
		{
			LOG_PRINTFLN("failed, rc=");
			LOG_PRINTFLN((const char *)client.state());
			LOG_PRINTFLN(" try again in 5 seconds");
			// Wait 5 seconds before retrying
			delay(5000);
		}
	}
	LOG_PRINTFLN("Publish message: ");
	String msg_a = "{'message':'hello'}";
	msg_a.toCharArray(msg, 50);
	LOG_PRINTFLN(msg);
	// Publish
	client.publish(brConfig.mfTopic, msg);
	LOG_PRINTFLN("Publish success");
	client.loop();
	delay(3000);
}
