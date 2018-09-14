/*  Interrupt-based DHT11 driver
 *  (c) 2018 Jylam
 */

#include <stdio.h>
#include <string.h>
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


#define DHT_EDGES_PER_READ (82)

struct {uint64_t ts; int value; }    edges[DHT_EDGES_PER_READ];

int dht_pin = -1;

uint64_t intr_start_time = 0;
int isr_count = 0;
int message_received = 0;

int dht_humidity = -1;
int dht_temp     = -1;

static void dht_setup_pin_output() {
    /* Configure GPIO as an output */
    gpio_config_t gpioConfig;
    gpioConfig.pin_bit_mask = 1<<dht_pin;
    gpioConfig.mode         = GPIO_MODE_OUTPUT;
    gpioConfig.pull_up_en   = GPIO_PULLUP_DISABLE;
    gpioConfig.pull_down_en = GPIO_PULLDOWN_ENABLE;
    gpioConfig.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&gpioConfig);
}

/* GPIO ISR */
static void IRAM_ATTR dht_isr_handler(void *args) {
    uint64_t t = esp_timer_get_time();
    int      v = gpio_get_level(dht_pin);

    edges[isr_count].ts = t;
    edges[isr_count].value = v;

    isr_count++;
    if((isr_count >= DHT_EDGES_PER_READ)) {
            gpio_uninstall_isr_service();
            dht_setup_pin_output();
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
    intr_start_time = esp_timer_get_time();
    gpio_isr_handler_add(dht_pin, dht_isr_handler, NULL);

    gpio_config(&gpioConfig);

    return 0;
}

/* Take 8 bits and put them in a byte */
static uint8_t dht_decode_byte(uint8_t *bits)
{
    uint8_t ret = 0;

    for (int i = 0; i < 8; ++i) {
        ret <<= 1;
        if (bits[i]) {
            ret++;
        }
    }
    return ret;
}


/* Wakeup the DHT11
 * Configure the GPIO to output, and set it to LOW for 20ms
 * */
static void dht_send_start()
{
    dht_setup_pin_output();
    /* Pull LOW for 20ms */
    gpio_set_level(dht_pin,0);
	ets_delay_us(20000);  // Should be at least 18

    /* The pin will be pulled up when setting the interrupt */
}

/* Get previously read temperature */
int dht_get_temperature(void) {
    return dht_temp;
}

/* Get previously read humidity */
int dht_get_humidity(void) {
    return dht_humidity;
}

/* Set the GPIO pin the DATA line of the DHT11 is plugged to */
void dht_set_pin(int pin)
{
	dht_pin = pin;
}

/* Send START, wait for completion, and compute the humidy and temperature */
int dht_get_data(void) {
    int ret;

    intr_start_time = 0;
    isr_count = 0;
    message_received = 0;

    memset(edges, 0, DHT_EDGES_PER_READ*sizeof(struct {uint64_t ts; int value; }));
    /* Wakeup the device */
    dht_send_start();
    /* Setup GPIO interrupt service */
    dht_setup_interrupt();

    while(!message_received) {
        if((esp_timer_get_time()-intr_start_time)>10000) {
            gpio_uninstall_isr_service();
            ret = DHT_TIMEOUT_ERROR;
            goto end;
        }

        ets_delay_us(1000);
    }
    int i;
    uint8_t bits[40] = {0x00};
    uint8_t  offset = 0;
    for(i = 3; i < DHT_EDGES_PER_READ; i++) {
        uint64_t diff = edges[i].ts-edges[i-1].ts;
        if(  (diff>40) && (diff < 60) ) {
        }
        else if(  (diff>15) && (diff < 35) ) {
            offset++;
        }
        else if(  (diff>=60) && (diff < 80) ) {
            bits[offset] = 1;
            offset++;
        } else {
            ret = DHT_SYNC_ERROR;
            goto end;
        }
    }

    uint8_t hum_int = dht_decode_byte(bits);
    uint8_t hum_dec = dht_decode_byte(&bits[8]);
    uint8_t temp_int = dht_decode_byte(&bits[16]);
    uint8_t temp_dec = dht_decode_byte(&bits[24]);
    uint8_t checksum = dht_decode_byte(&bits[32]);

    if (((hum_int + hum_dec + temp_int + temp_dec) & 0xff) != checksum) {
        ret = DHT_CHECKSUM_ERROR;
    } else {
        dht_humidity = (hum_int*1000)+hum_dec;
        dht_temp = (temp_int*1000)+temp_dec;
        ret = DHT_OK;
    }
end:
    gpio_uninstall_isr_service();

    return ret;
}
