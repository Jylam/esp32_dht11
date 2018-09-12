/*  DHT11 driver
 *  (c) 2018 Jylam
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
#include "time.h"
#include <sys/time.h>

int dht_pin = -1;

uint64_t get_time_us(void) {
	struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };
	uint32_t sec, us;
	gettimeofday(&tv, NULL);
	(sec) = tv.tv_sec;
	(us) = tv.tv_usec;
	return (sec*1000000)+us;
}

void dht_set_pin(int pin)
{
	dht_pin = pin;
}

/* Wait for HIGH or LOW on dht_pin and returns 1 (or 0 for timeout) */
int dht_wait_for(int v, uint64_t timeout) {
    uint64_t start_time = get_time_us();
    uint64_t diff = 0;

	while(diff < timeout) {
        if(!gpio_get_level(dht_pin)) {
            return 1;
        }
        diff = (get_time_us()-start_time);
    }
    return 0;
}

/* Get a bit from DHT11
 * Signal is LOW for 50us, then HIGH for either 26/28us (0) or 70us (1)
 * */
int dht_get_bit(void) {
    /* Wait for the line to be pulled LOW by the DHT11 (max 50us) */
    if(!dht_wait_for(0, 50)) {
        return 0;
    }

    return 0;
}


/* Wakeup the DHT11
 * Set pin LOW for at leat 18ms, then HIGH and wait for HIGH for at most 80us
 * */
int dht_send_start()
{
    printf("Sending START\n");
    /* Configure pin for output  */
	gpio_set_direction(dht_pin, GPIO_MODE_OUTPUT);

    /* Pull LOW for 20ms */
    gpio_set_level(dht_pin,0);
	ets_delay_us(20000);

    /* Pull HIGH then reconfigure to input with a pullup */
	gpio_set_level(dht_pin,1);
	gpio_set_direction(dht_pin, GPIO_MODE_INPUT);
	gpio_pullup_en(dht_pin);

    /* Wait for the line to be pulled LOW by the DHT11 (max 40us) */
    if(!dht_wait_for(0, 40)) {
        return 0;
    }
    /* Wait for the line to be pulled HIGH by the DHT11 (max 80us) */
    if(!dht_wait_for(1, 80)) {
        return 0;
    }

    return 1;
}

int dht_get_data(void) {
    int ret = 1;
    int i;
    /* Wakeup the device */
    if(!dht_send_start()) {
        printf("Device not responding\n");
        return -1;
    }
    /* Read 40 bits of data */
    uint8_t bits[40] = {0};
    for(i=0; i<40; i++) {
        bits[i] = dht_get_bit();
    }

    for(i=0; i<40; i++) {
        printf("%d\n", bits[i]);
    }
    return ret;
}
