#ifndef _MQTT_H_
#define _MQTT_H_

#define MQTT_BROKER "broker.hivemq.com"
#define TOPIC_SENSOR "mandevices/sensor"
#define TOPIC_FOTA "mandevices/fota"
#define TOPIC_PROCESS "mandevices/process"
#define TOPIC_NUMBER "mandevices/number"

void mqtt_client_sta(void);

#endif