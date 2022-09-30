#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_now.h"

#include "nvs_flash.h"

#define WIFI_SSID               "Wifi free"
#define WIFI_PASSWORD           "99999999"
#define WIFI_CONNECTED_BIT      BIT0
static const char *espnow_tag = "ESPNOW_TASK";
static const char *espnow_cb_tag = "ESPNOW_CB";
uint8_t responder1_mac_addr[ESP_NOW_ETH_ALEN] = {0x94, 0xB9, 0x7E, 0xE4, 0x9E, 0xD4};
static EventGroupHandle_t event_group;

static void mac_to_str(char *buff, uint8_t *mac)
{
    sprintf(buff, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        esp_wifi_connect();
        ESP_LOGI(espnow_cb_tag, "Retry...");
        xEventGroupClearBits(event_group, WIFI_CONNECTED_BIT);
    }
    else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        ESP_LOGI(espnow_cb_tag, "WiFi connected"); 
    }
    else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        xEventGroupSetBits(event_group, WIFI_CONNECTED_BIT);
    }
    else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(espnow_cb_tag, "WiFi connecting...");
        esp_wifi_connect();
    }
}

void espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
    char buff[13] = {0};
    mac_to_str(buff, (uint8_t*)mac_addr);
    ESP_LOGI(espnow_cb_tag, "Recv message from %s", buff);
    ESP_LOGI(espnow_cb_tag, "%.*s\n", data_len, data);
}

void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    char mac_addr_str[13] = {0};
    mac_to_str(mac_addr_str, (uint8_t*)mac_addr);
    switch(status)
    {
        case ESP_NOW_SEND_SUCCESS:
            ESP_LOGI(espnow_cb_tag, "Espnow send success to %s",mac_addr_str);
            break;
        case ESP_NOW_SEND_FAIL:
            ESP_LOGE(espnow_cb_tag, "Espnow send fail to %s",mac_addr_str);
            break;
    }
}

static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // esp_netif_create_default_wifi_sta();
    // ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    // ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    //     wifi_config_t wifi_sta_config = {
    //     .sta = {
    //         .ssid = WIFI_SSID,
    //         .password = WIFI_PASSWORD,
    //         .bssid_set = false
    //     }
    // };
    // esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_sta_config);
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_LR);
    // xEventGroupWaitBits(event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    // ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler));
    // ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler));
}

static void espnow_init(void)
{
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
}

void espnow_send_task(void)
{
    char send_buff[250] = {0}; 
    int i = 0; 
    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(esp_now_peer_info_t));
    memcpy(peer.peer_addr, responder1_mac_addr, ESP_NOW_ETH_ALEN);
    peer.channel = 2;
    esp_now_add_peer(&peer);
    while(1)
    {
        sprintf(send_buff, "Message %d: Hello", i);
        i++;
        ESP_ERROR_CHECK(esp_now_send(NULL, (uint8_t *)send_buff, strlen(send_buff)));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    uint8_t my_mac_addr[ESP_NOW_ETH_ALEN] = {0};
    char my_mac_addr_str[20] = {0};
    esp_efuse_mac_get_default(my_mac_addr);
    mac_to_str(my_mac_addr_str, my_mac_addr);
    ESP_LOGI(espnow_tag, "My MAC %s", my_mac_addr_str);

    ESP_ERROR_CHECK(nvs_flash_init());
    event_group = xEventGroupCreate();
    wifi_init();
    espnow_init();
    // xTaskCreate(espnow_send_task, "espnow_send_task", 2048, NULL, 5, NULL);
}