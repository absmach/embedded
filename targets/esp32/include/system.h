#if !defined(OBJECTS_H)
#define OBJECTS_H

#include <Arduino.h>
#include <MqttClient.h>

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

#endif