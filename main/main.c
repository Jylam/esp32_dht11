/* Main Swarm32 file
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "rom/ets_sys.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include <sys/time.h>
#include "sdkconfig.h"
#include "esp_http_client.h"

#include "dht11.h"
#include "wifi.h"
#include "sntp.h"



#define SLEEP_TIME_S  10//(10*60)
#define SLEEP_TIME_US (SLEEP_TIME_S*1000*1000)

uint64_t get_time_us(void) {
    struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };
    uint32_t sec, us;
    gettimeofday(&tv, NULL);
    (sec) = tv.tv_sec;
	(us) = tv.tv_usec;
	return (sec*1000000)+us;
}

struct dht_result {
    int temp;
    int humi;
    bool valid;
};

int temp_done = 0;
void DHT_task(void *pvParameter) {
    struct dht_result *res = (struct dht_result*) pvParameter;
    int avg_temp = 0;
    int avg_humi = 0;
    int valid_count = 0;

    res->valid = false;

    for(int i=0; i<5; i++ ) {
        if(dht_get_data() == DHT_OK) {
            int temp = dht_get_temperature();
            int humi = dht_get_humidity();
            avg_temp += temp;
            avg_humi += humi;
            valid_count+=1;
        }
        vTaskDelay(2000 / portTICK_RATE_MS);
    }


    if(valid_count) {
        res->temp = avg_temp/valid_count;
        res->humi = avg_humi/valid_count;
        res->valid = true;
    }
    vTaskDelete(NULL);
}

void SNTP_task(void *ptr) {
    initialize_sntp();
    vTaskDelete(NULL);
}

void app_main() {
    TaskHandle_t dht_handle;
    TaskHandle_t sntp_handle;
    struct dht_result dht_result;


    printf("Swarm32 (c) Jylam 2018\n");

    nvs_flash_init();

    // Init wifi (returns almost immediately)
    wifi_init();
    // Run SNTP task
    xTaskCreate(&SNTP_task, "SNTP_task", 2048, NULL, configMAX_PRIORITIES, &sntp_handle);

    // Configure DHT
	dht_set_pin(GPIO_NUM_27);
	dht_set_type(DHT_TYPE11);

    // Wait a bit, the DHT needs time to init
    vTaskDelay(1000 / portTICK_RATE_MS);
    // Run DHT task
    xTaskCreate(&DHT_task, "DHT_task", 20480, &dht_result, configMAX_PRIORITIES, &dht_handle);
	vTaskDelay(100 / portTICK_RATE_MS);

    // Wait for DHT task completion
    while(eTaskGetState(dht_handle) != eReady) { // ??? should be eDelete ?
        vTaskDelay(500 / portTICK_RATE_MS);
    }
    if(dht_result.valid) {
        printf("Got measurement : %f %f\n", dht_result.temp/1000.0, dht_result.humi/1000.0);
    }

    printf("Sleeping for %fs...\n", SLEEP_TIME_US/1000000.0);
    fflush(stdout);


    wifi_deinit();

    esp_deep_sleep(SLEEP_TIME_US);
}
