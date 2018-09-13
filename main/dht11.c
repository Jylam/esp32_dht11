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
#include "xtensa/core-macros.h"


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
static void IRAM_ATTR dht_isr_handler(void *args) {
    uint64_t t = esp_timer_get_time();
    int      v = gpio_get_level(dht_pin);

    edges[isr_count].ts = t;
    edges[isr_count].value = v;

    isr_count++;
    if((isr_count >= DHT11_EDGES_PER_READ)) {
            gpio_uninstall_isr_service();
            message_received = 1;
    }
}

/* Configure GPIO for interruptions on both edges */
static inline int dht_setup_interrupt(void) {
    gpio_config_t gpioConfig;
    gpioConfig.pin_bit_mask = 1<<dht_pin;
    gpioConfig.mode         = GPIO_MODE_INPUT;
    gpioConfig.pull_up_en   = GPIO_PULLUP_DISABLE;
    gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpioConfig.intr_type    = GPIO_INTR_ANYEDGE;

    message_received = 0;
    isr_count        = 0;
    gpio_install_isr_service(0);
    intr_start_time = xthal_get_ccount();//esp_timer_get_time();
    gpio_isr_handler_add(dht_pin, dht_isr_handler, NULL);

    gpio_intr_disable(dht_pin);
    gpio_config(&gpioConfig);

    return 0;
}


void dht_set_pin(int pin)
{
	dht_pin = pin;
}



/* Wakeup the DHT11
 * Set pin LOW for at leat 18ms, then HIGH and wait for HIGH for at most 80us
 * */
static int dht_send_start()
{
    printf("Sending START\n");
    /* Configure pin for output  */
	gpio_set_direction(dht_pin, GPIO_MODE_OUTPUT);

    /* Pull LOW for 20ms */
    gpio_set_level(dht_pin,0);
	ets_delay_us(20000);  // Should be at least 18
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

    while(!message_received) {
        printf("%d itr\n", isr_count);
        ets_delay_us(1000);
    }
    printf("Message received, %d interruptions\n", isr_count);
    int i;
    for(i = 1; i < DHT11_EDGES_PER_READ; i++) {
        uint64_t diff = edges[i].ts-edges[i-1].ts;
        printf("%llu: %d  (diff %llu)\t", edges[i].ts, edges[i].value, diff);
        if(  (diff>40) && (diff < 60) ) {
            printf("\n");
        }
        else if(  (diff>20) && (diff < 30) ) {
            printf("0\n");
        }
        else if(  (diff>60) && (diff < 80) ) {
            printf("1\n");
        } else {
            printf("?????\n");
        }
    }
    return ret;
}
