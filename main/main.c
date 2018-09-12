/* Main Swarm32 file
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "rom/ets_sys.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#include "dht11.h"

void print_temp_humid(void) {
	dht_set_pin(GPIO_NUM_27);
    printf("Read: %d\n", dht_get_data());
}


void DHT_task(void *pvParameter)
{
	printf("Starting DHT measurement!\n");
	while(1)
	{
		print_temp_humid();
		vTaskDelay(5000 / portTICK_RATE_MS);
	}
}



void app_main()
{
	printf("Swarm32 (c) Jylam 2018\n");

	nvs_flash_init();
	vTaskDelay(1000 / portTICK_RATE_MS);
	//xTaskCreate(&DHT_task, "DHT_task", 2048, NULL, 5, NULL);
	print_temp_humid();


	//    vTaskDelay(1000 / portTICK_PERIOD_MS);
	printf("Sleeping for 5s...\n");
	fflush(stdout);
	esp_deep_sleep(5000000);
}
