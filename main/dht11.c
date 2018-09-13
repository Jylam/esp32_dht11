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

#define DHT11_BITS_PER_READ  40
#define DHT11_EDGES_PREAMBLE 2
//#define DHT11_EDGES_PER_READ (2 * DHT11_BITS_PER_READ)
#define DHT11_EDGES_PER_READ (80)

struct {uint64_t ts; int value; }    edges[DHT11_EDGES_PER_READ];

int dht_pin = -1;

uint64_t intr_start_time = 0;
int isr_count = 0;
int message_received = 0;
/* GPIO ISR */
static void dht_isr_handler(void *args) {
    uint64_t t =  esp_timer_get_time();
    int      v = gpio_get_level(dht_pin);

    edges[isr_count].ts = t;
    edges[isr_count].value = v;

    isr_count++;
    if((isr_count >= DHT11_EDGES_PER_READ) || ((t-intr_start_time)>100000)) {
            gpio_uninstall_isr_service();
            message_received = 1;
    }
}

/* Configure GPIO for interruptions on both edges */
int dht_setup_interrupt(void) {
    gpio_config_t gpioConfig;
    gpioConfig.pin_bit_mask = 1<<dht_pin;
    gpioConfig.mode         = GPIO_MODE_INPUT;
    gpioConfig.pull_up_en   = GPIO_PULLUP_DISABLE;
    gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpioConfig.intr_type    = GPIO_INTR_ANYEDGE;
    gpio_config(&gpioConfig);

    return 0;
}
int dht_enable_interrupt(void) {
    message_received = 0;
    isr_count        = 0;
    gpio_install_isr_service(0);
    intr_start_time = esp_timer_get_time();
    gpio_isr_handler_add(dht_pin, dht_isr_handler, NULL   );

    return 0;
}






void dht_set_pin(int pin)
{
	dht_pin = pin;
}

/* Wait for HIGH or LOW on dht_pin and returns 1 (or 0 for timeout) */
int dht_wait_for(int v, uint64_t timeout) {
    uint64_t start_time = esp_timer_get_time();
    uint64_t diff = 0;

	while(diff < timeout) {
        if(!gpio_get_level(dht_pin)) {
            return 1;
        }
        diff = (esp_timer_get_time()-start_time);
    }
    return 0;
}

/* Get a bit from DHT11
 * Signal is LOW for 50us, then HIGH for either 26/28us (0) or 70us (1)
 * */
int dht_get_bit(void) {

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
#if 0
    /* Wait for the line to be pulled LOW by the DHT11 (max 40us) */
    if(!dht_wait_for(0, 40)) {
        return 0;
    }
    /* Wait for the line to be pulled HIGH by the DHT11 (max 80us) */
    if(!dht_wait_for(1, 80)) {
        return 0;
    }
#endif
    return 1;
}

int dht_get_data(void) {
    int ret = 1;

    for(int i = 0; i < DHT11_EDGES_PER_READ; i++) {
        edges[i].ts    = 0;
        edges[i].value = 0;
    }

    /* Wakeup the device */
    if(!dht_send_start()) {
        printf("Device not responding\n");
        return -1;
    }
    /* Setup GPIO interrupt service */
    dht_setup_interrupt();
    /* Enable interrupt */
    dht_enable_interrupt();

#if 0
    int i;
    /* Read 40 bits of data */
    uint8_t bits[40] = {0};
    for(i=0; i<40; i++) {
        bits[i] = dht_get_bit();
    }

    for(i=0; i<40; i++) {
        printf("%d\n", bits[i]);
    }
#endif

    while(!message_received) {

    }
    printf("Message received\n");
    int i;
    for(i = 1; i < DHT11_EDGES_PER_READ; i++) {
        printf("%llu: %d  (diff %llu)\n", edges[i].ts, edges[i].value, edges[i].ts-edges[i-1].ts);
    }
    return ret;
}
