/*****
 * Test Manflux PubSub with MQTT ESP32 Client
******************/

#include <Arduino.h>
#include <WiFiEsp.h>
#include <WiFiEspClient.h>
#include <WiFiEspUdp.h>
#include <MqttClient.h>

#include "config.h"

// Enable MqttClient logs
#define MQTT_LOG_ENABLED 1

#define LOG_PRINTFLN(fmt, ...) logfln(fmt, ##__VA_ARGS__)
#define LOG_SIZE_MAX 128
void logfln(const char *fmt, ...)
{
	char buf[LOG_SIZE_MAX];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, LOG_SIZE_MAX, fmt, ap);
	va_end(ap);
	Serial.println(buf);
}

#define HW_UART_SPEED 115200L
#define MQTT_ID "STM32"
// Initialize second UART on STM32 client object
HardwareSerial Serial2(USART2);

// Initailize MQTT client on device
WiFiEspClient espClient;

// ============== Object to supply system functions ============================
class System : public MqttClient::System
{
public:
	unsigned long millis() const
	{
		return ::millis();
	}

	void yield(void)
	{
		::yield();
	}
};

void processMessage(MqttClient::MessageData &md)
{
	const MqttClient::Message &msg = md.message;
	char payload[msg.payloadLen + 1];
	memcpy(payload, msg.payload, msg.payloadLen);
	payload[msg.payloadLen] = '\0';
	LOG_PRINTFLN(
		"Message arrived: qos %d, retained %d, dup %d, packetid %d, payload:[%s]",
		msg.qos, msg.retained, msg.dup, msg.id, payload);
}


void setup()
{
	// Setup hardware serial for logging
	Serial.begin(HW_UART_SPEED);
	while (!Serial)
		;

	// Setup WiFi network
	WiFi.mode(WIFI_STA);
	WiFi.hostname("ESP_" MQTT_ID);
	WiFi.begin(wifi_config.wifi_ssid, wifi_config.wifi_pass);
	LOG_PRINTFLN("\n");
	LOG_PRINTFLN("Connecting to WiFi");
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		LOG_PRINTFLN(".");
	}
	LOG_PRINTFLN("Connected to WiFi");
	LOG_PRINTFLN("IP: %s", WiFi.localIP().toString().c_str());

	// Setup MqttClient
	MqttClient::System *mqttSystem = new System;
	MqttClient::Logger *mqttLogger = new MqttClient::LoggerImpl<HardwareSerial>(Serial);
	MqttClient::Network *mqttNetwork = new MqttClient::NetworkClientImpl<WiFiClient>(network, *mqttSystem);
	//// Make 128 bytes send buffer
	MqttClient::Buffer *mqttSendBuffer = new MqttClient::ArrayBuffer<128>();
	//// Make 128 bytes receive buffer
	MqttClient::Buffer *mqttRecvBuffer = new MqttClient::ArrayBuffer<128>();
	//// Allow up to 2 subscriptions simultaneously
	MqttClient::MessageHandlers *mqttMessageHandlers = new MqttClient::MessageHandlersImpl<2>();
	//// Configure client options
	MqttClient::Options mqttOptions;
	////// Set command timeout to 10 seconds
	mqttOptions.commandTimeoutMs = 10000;
	//// Make client object
	mqtt = new MqttClient(
		mqttOptions, *mqttLogger, *mqttSystem, *mqttNetwork, *mqttSendBuffer,
		*mqttRecvBuffer, *mqttMessageHandlers);
}


void loop()
{
	// Check connection status
	if (!mqtt->isConnected())
	{
		// Close connection if exists
		network.stop();
		// Re-establish TCP connection with MQTT broker
		LOG_PRINTFLN("Connecting");
		network.connect(server, 1883);
		if (!network.connected())
		{
			LOG_PRINTFLN("Can't establish the TCP connection");
			delay(5000);
		}
		else
		{
			LOG_PRINTFLN("Connected to MQTT broker connected");
		}
		// Start new MQTT connection
		MqttClient::ConnectResult connectResult;
		// Connect
		{
			MQTTPacket_connectData options = MQTTPacket_connectData_initializer;
			options.MQTTVersion = 4;
			options.clientID.cstring = (char *)MQTT_ID;
			options.username.cstring = (char *)br_config.mf_thing_id;
			options.password.cstring = (char *)br_config.mf_thing_pass;
			options.cleansession = true;
			options.keepAliveInterval = 15; // 15 seconds
			MqttClient::Error::type rc = mqtt->connect(options, connectResult);
			if (rc != MqttClient::Error::SUCCESS)
			{
				LOG_PRINTFLN("Connection error: %i", rc);
				return;
			}
		}
		{	//Subscribe
			MqttClient::Error::type rc = mqtt->subscribe(
				br_config.mf_topic, MqttClient::QOS0, processMessage);
			if (rc != MqttClient::Error::SUCCESS)
			{
				LOG_PRINTFLN("Subscribe error: %i", rc);
				LOG_PRINTFLN("Drop connection");
				mqtt->disconnect();
				return;
			}
		}
	}
	else
	{
		// {   // Publish
		// 	const char *buf = "{'message': 'hello'}";
		// 	MqttClient::Message message;
		// 	message.qos = MqttClient::QOS0;
		// 	message.retained = false;
		// 	message.dup = false;
		// 	message.payload = (void *)buf;
		// 	message.payloadLen = strlen(buf);
		// 	MqttClient::Error::type rc = mqtt->publish(br_config.mf_topic, message);
		// 	if (rc != MqttClient::Error::SUCCESS)
		// 	{
		// 		LOG_PRINTFLN("Publish error: %i", rc);
		// 	}
		// 	else
		// 	{
		// 		LOG_PRINTFLN("Publish success");
		// 	}
		// }
		// Idle for 30 seconds
		mqtt->yield(30000L);
	}
}


