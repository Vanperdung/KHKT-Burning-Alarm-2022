#include <stdio.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"

#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_wifi.h"
#include "esp_tls.h"
#include "esp_smartconfig.h"
#include "esp_attr.h"
#include "mqtt_client.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/spi_master.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "cJSON.h"

#include "button.h"
#include "fota.h"
#include "led.h"
#include "lora.h"
#include "mqtt.h"
#include "sim.h"
#include "smartconfig.h"
#include "wifi.h"
#include "main.h"
#include "spiffs_user.h"

static const char *TAG = "MQTT";
extern uint8_t topic_room_1_sensor[100];
extern uint8_t topic_room_2_sensor[100];
extern uint8_t topic_room_3_sensor[100];
extern uint8_t topic_room_4_sensor[100];
extern uint8_t topic_fota[100];
extern uint8_t topic_process[100];
extern uint8_t topic_number[100];
RingbufHandle_t mqtt_ring_buf;
esp_mqtt_client_handle_t client;  

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
	ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
	esp_mqtt_event_handle_t event = event_data;
	esp_mqtt_client_handle_t client = event->client;
	switch((esp_mqtt_event_id_t)event_id) 
    {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT event connected");
            esp_mqtt_client_subscribe(client, (char*)topic_fota, 0);
            esp_mqtt_client_subscribe(client, (char*)topic_number, 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT event disconnected");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT event subcribed, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT event unsubcribed, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT event published, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
        {
            UBaseType_t res = xRingbufferSend(mqtt_ring_buf, event->data, event->data_len, portMAX_DELAY);
            if(res != pdTRUE) 
                ESP_LOGE(TAG, "Failed to send item\n");
            break;
        }
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT event error");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
	}
}

static void process_message_json(char *mess, _message_object *mess_obj)
{
    cJSON *root = cJSON_Parse(mess);
    cJSON *current_obj = NULL;
    cJSON_ArrayForEach(current_obj, root)
    {
        if(current_obj->string)
        {
            const char *str = current_obj->string;
            if(strcmp(str, "action") == 0)
                memcpy(mess_obj->action, current_obj->valuestring, strlen(current_obj->valuestring) + 1);
            if(strcmp(str, "url") == 0)
                memcpy(mess_obj->url, current_obj->valuestring, strlen(current_obj->valuestring) + 1);
            if(strcmp(str, "number") == 0)
                memcpy(mess_obj->number, current_obj->valuestring, strlen(current_obj->valuestring) + 1);
        }
    }
    cJSON_Delete(root);
}

static void mqtt_task(void *param)
{
    _message_object mess_obj;
    char *mess_recv = NULL;
    size_t mess_size = 0;
    while(1)
    {
        mess_recv = (char*)xRingbufferReceive(mqtt_ring_buf, &mess_size, portMAX_DELAY);
        {
            if(mess_recv)
            {
                mess_recv[mess_size] = '\0';
                ESP_LOGI(TAG, "Payload: %s", mess_recv);
                memset(&mess_obj, 0, sizeof(mess_obj));
                process_message_json(mess_recv, &mess_obj);
                if(strcmp(mess_obj.action, "fota_device") == 0)
                {
                    if(strlen(mess_obj.url) < 10)
                        ESP_LOGE(TAG, "URL fota not correct");
                    else
                        xTaskCreate(&fota_task, "fota_task", 8192, mess_obj.url, 15, NULL);
                }
                if(strcmp(mess_obj.action, "update_number") == 0)
                {
                    if(strlen(mess_obj.number) < 5)
                        ESP_LOGE(TAG, "Number not correct");
                    else
                    {
                        ESP_LOGI(TAG, "Update number: %s", mess_obj.number);
                        write_to_file("number.txt", mess_obj.number);
                    }
                }
            }
        }
    }
}

void mqtt_client_sta(void)
{
    uint8_t broker[50] = {0};  
    ESP_LOGI(TAG, "MQTT init");
    ESP_LOGI(TAG, "Broker: %s", MQTT_BROKER);
    sprintf((char*)broker, "mqtt://%s", MQTT_BROKER);
    strcpy((char*)topic_room_1_sensor, TOPIC_ROOM_1_SENSOR);
    strcpy((char*)topic_room_2_sensor, TOPIC_ROOM_2_SENSOR);
    strcpy((char*)topic_room_3_sensor, TOPIC_ROOM_3_SENSOR);
    strcpy((char*)topic_room_4_sensor, TOPIC_ROOM_4_SENSOR);
    strcpy((char*)topic_fota, TOPIC_FOTA);
    strcpy((char*)topic_process, TOPIC_PROCESS);
    strcpy((char*)topic_number, TOPIC_NUMBER);
    mqtt_ring_buf = xRingbufferCreate(4096, RINGBUF_TYPE_NOSPLIT);
    if(mqtt_ring_buf == NULL)
        ESP_LOGE(TAG, "Failed to create ring buffer");
    // memcpy(mqtt_cfg.uri, broker, sizeof(broker));
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = (char*)broker,
        .keepalive = 60
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
	esp_mqtt_client_start(client);
    xTaskCreate(&mqtt_task, "mqtt_task", 8192, NULL, 9, NULL);
}