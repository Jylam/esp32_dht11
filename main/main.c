/* Main Swarm32 file
*/
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp32/rom/ets_sys.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include <sys/time.h>
#include "sdkconfig.h"
#include "esp_http_client.h"
#include "esp32/rom/rtc.h"
#include "esp_sleep.h"


#include "wifi.h"
#include "sntp.h"



#define SLEEP_TIME_S  ((uint64_t)(60*60))
#define SLEEP_TIME_US ((uint64_t)(SLEEP_TIME_S*1000*1000))

uint64_t get_time_us(void) {
    struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };
    uint32_t sec, us;
    gettimeofday(&tv, NULL);
    (sec) = tv.tv_sec;
    (us) = tv.tv_usec;
    return (sec*1000000)+us;
}


void SNTP_task(void *ptr) {
    initialize_sntp();
    vTaskDelete(NULL);
}

esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    //printf("HTTP EVENT %d\n", evt->event_id);
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            break;
        case HTTP_EVENT_ON_CONNECTED:
            break;
        case HTTP_EVENT_HEADER_SENT:
            break;
        case HTTP_EVENT_ON_HEADER:
            //printf("%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            //if (!esp_http_client_is_chunked_response(evt->client)) {
            //    printf("%.*s", evt->data_len, (char*)evt->data);
            //}
            printf("Got answer from the HTTP server\n");
            break;
        case HTTP_EVENT_ON_FINISH:
            break;
        case HTTP_EVENT_DISCONNECTED:
            break;
    }
    return ESP_OK;
}

char unique_id[13]; // MAC is 6 bytes, in hex 12 bytes + trailing \0 -> 13 bytes
char *get_unique_id_str(void) {
    uint8_t mac[6] = {0};
    esp_err_t err = esp_efuse_mac_get_default(mac);
    if(err != ESP_OK) {
        memset(unique_id, 0, sizeof(unique_id));
        return NULL;
    }

    snprintf(unique_id, 13, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return unique_id;
}


void app_main() {
    TaskHandle_t sntp_handle;

    esp_reset_reason_t reset_reason = rtc_get_reset_reason(0); // 1: power-on    5: deep-sleep


    printf("Swarm32-plants (c) Jylam 2018-2019\n");
    nvs_flash_init();

    // Init wifi (returns almost immediately)
    wifi_init();
    // Run SNTP task
    xTaskCreate(&SNTP_task, "SNTP_task", 2048, NULL, configMAX_PRIORITIES, &sntp_handle);
    // Wait for the task to be created and SNTP stack to be initialized FIXME
    vTaskDelay(500 / portTICK_RATE_MS);
    // Wait for SNTP
    int got_sntp = xEventGroupWaitBits(
            sntp_event_group ,
            SNTP_SUCCESS_BIT | SNTP_FAILURE_BIT,
            pdFALSE,        // Don't clear bit after waiting
            pdFALSE,
            20000 / portTICK_PERIOD_MS);

    // Got NTP date, which also means we have a wifi connection and some sort of internet access
    if(got_sntp & SNTP_SUCCESS_BIT) {
        char strftime_buf[64];
        struct tm timeinfo = { 0 };
        setenv("TZ", "GMT+2/-2,M10.5.0/-1", 1);
        tzset();
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        printf("The current date/time is: %s (%d)\n", strftime_buf, (unsigned int)now);

        char post_data[256] = "";
        snprintf(post_data, 256, "http://frob.fr/SWARM32/?id=%s&time=%u&gnd_humi=%d&reset=%d", get_unique_id_str() ,(unsigned int)now, -1, reset_reason);

        /* Send HTTP message*/
        esp_http_client_config_t config = {
            .url = post_data,
            .event_handler = http_event_handler,
        };


        esp_http_client_handle_t client = esp_http_client_init(&config);

        //esp_http_client_set_method(client, HTTP_METHOD_POST);
        //esp_http_client_set_post_field(client, post_data, strlen(post_data));

        esp_err_t err = esp_http_client_perform(client);

        if (err == ESP_OK) {
            printf("Status = %d, content_length = %d\n",
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
        }

        esp_http_client_cleanup(client);
    }
    else if(got_sntp & SNTP_FAILURE_BIT) {
        printf("Can't get NTP date\n");
    } else {
        printf("SNTP Timeout\n");
    }

    printf("Sleeping for %fs...\n", SLEEP_TIME_US/1000000.0);
    fflush(stdout);

    //    wifi_deinit();

    esp_deep_sleep(SLEEP_TIME_US);
}
