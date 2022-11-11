#ifndef _MQTT_H_
#define _MQTT_H_

#define MQTT_BROKER         "broker.hivemq.com"
#define TOPIC_ROOM_1_SENSOR "mandevices/room_1/sensor"
#define TOPIC_ROOM_2_SENSOR "mandevices/room_2/sensor"
#define TOPIC_ROOM_3_SENSOR "mandevices/room_3/sensor"
#define TOPIC_ROOM_4_SENSOR "mandevices/room_4/sensor"
#define TOPIC_FOTA          "mandevices/fota"
#define TOPIC_PROCESS       "mandevices/process"
#define TOPIC_NUMBER        "mandevices/number"
#define TOPIC_MESSAGE       "mandevices/message"

void mqtt_client_sta(void);

#endif