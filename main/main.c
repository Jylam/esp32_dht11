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

#include "dht11.h"

#define SLEEP_TIME_S  1//(10*60)
#define SLEEP_TIME_US (SLEEP_TIME_S*1000*1000)

uint64_t get_time_us(void) {
    struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };
    uint32_t sec, us;
    gettimeofday(&tv, NULL);
    (sec) = tv.tv_sec;
	(us) = tv.tv_usec;
	return (sec*1000000)+us;
}


void print_temp_humid(void) {
	dht_set_pin(GPIO_NUM_27);
    int ret = dht_get_data();
    if(ret == DHT_OK ) {
        printf("Temp: %d\n", dht_get_temperature());
        printf("Humi: %d\n", dht_get_humidity());
    } else {
        printf("Failed to get data (error %d)\n", ret);
    }
}

int temp_done = 0;
void DHT_task(void *pvParameter)
{
	printf("Starting DHT measurement!\n");
    for(int i=0; i<10; i++ ) {
        print_temp_humid();
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
    temp_done = 1;
    vTaskDelete(NULL);

}


void app_main()
{
	printf("Swarm32 (c) Jylam 2018\n");

	nvs_flash_init();
	vTaskDelay(1000 / portTICK_RATE_MS);
	xTaskCreate(&DHT_task, "DHT_task", 20480, NULL, 5, NULL);

    printf("Waiting...\n");
    while(!temp_done) {
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
    printf("Sleeping for %fs...\n", SLEEP_TIME_US/1000000.0);
    fflush(stdout);
    esp_deep_sleep(SLEEP_TIME_US);
}
