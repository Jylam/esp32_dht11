#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "wifi.h"


EventGroupHandle_t wifi_event_group;


esp_err_t event_handler(void *ctx, system_event_t *event)
{
    //printf("Wifi Event ID : %08X\n", event->event_id);
    switch(event->event_id) {
        case SYSTEM_EVENT_STA_CONNECTED:
            printf("Connected !\n");
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            printf("Disconnected\n");
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            if(xEventGroupGetBits(wifi_event_group) & CONNECTION_NEEDED_BIT) {
                esp_wifi_connect();
            }
            break;
        case SYSTEM_EVENT_STA_START:
            break;
        case SYSTEM_EVENT_STA_STOP:
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            printf("Got IP !\n");
            break;
        default:
            printf("Unknown event\n");
    }

    return ESP_OK;
}

int wifi_init(void) {
    wifi_event_group = xEventGroupCreate();

    xEventGroupSetBits(wifi_event_group, CONNECTION_NEEDED_BIT);

    tcpip_adapter_init();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    wifi_config_t sta_config = {
        .sta = {
            .ssid = "Revolution",
            .password = "anneanne",
            .bssid_set = false
        }
    };
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_ERROR_CHECK( esp_wifi_connect() );

    return 0;
}

int wifi_deinit(void) {
    xEventGroupClearBits(wifi_event_group, CONNECTION_NEEDED_BIT);
    esp_wifi_disconnect();
    esp_wifi_stop();
    return 0;
}


