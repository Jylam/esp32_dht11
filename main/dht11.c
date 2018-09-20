/*  Interrupt-based DHT11 driver
 *  (c) 2018 Jylam
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "rom/ets_sys.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "dht11.h"
#include "time.h"
#include "xtensa/core-macros.h"


#define DHT_EDGES_PER_READ (83)
#define DHT_RESPONSE_TIMEOUT_US 10000

static void dht_isr_handler(void *args);

struct {uint64_t ts; int value; }    edges[DHT_EDGES_PER_READ];
SemaphoreHandle_t message_semaphore = NULL;
StaticSemaphore_t message_semaphore_buffer;

int      dht_pin          = -1;
uint64_t intr_start_time  = 0;
int      isr_count        = 0;
int      dht_humidity     = -1;
int      dht_temp         = -1;


/* Configure GPIO as an output, no interruption */
static void IRAM_ATTR dht_setup_pin_output() {
    gpio_config_t gpioConfig;

    gpioConfig.pin_bit_mask = 1<<dht_pin;
    gpioConfig.mode         = GPIO_MODE_OUTPUT;
    gpioConfig.pull_up_en   = GPIO_PULLUP_DISABLE;
    gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpioConfig.intr_type    = GPIO_INTR_DISABLE;

    gpio_config(&gpioConfig);
}

/* Configure GPIO as an input for interruptions on both edges */
static inline int IRAM_ATTR dht_setup_interrupt(void) {
    gpio_config_t gpioConfig;

    /* Reset */
    isr_count        = 0;
    intr_start_time = esp_timer_get_time();

    /* configure GPIO as an input, interrupted on both raising and falling edges */
    gpioConfig.pin_bit_mask = 1<<dht_pin;
    gpioConfig.mode         = GPIO_MODE_INPUT;
    /* Pull-up is not necessary, and will cause a short circuit,
     * as the DHT11 is pulling down at the same time */
    gpioConfig.pull_up_en   = GPIO_PULLUP_ENABLE;
    gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpioConfig.intr_type    = GPIO_INTR_ANYEDGE;


    /* Setup ISR service and install the ISR itself */
    gpio_install_isr_service(ESP_INTR_FLAG_EDGE);
    gpio_isr_handler_add(dht_pin, dht_isr_handler, NULL);

    gpio_config(&gpioConfig);

    return 0;
}


/* GPIO ISR */
static void IRAM_ATTR dht_isr_handler(void *args) {
    uint64_t t = esp_timer_get_time();
    int      v = gpio_get_level(dht_pin);

    edges[isr_count].ts = t;
    edges[isr_count].value = v;

    isr_count++;
    if((isr_count >= DHT_EDGES_PER_READ)) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            gpio_uninstall_isr_service();
            dht_setup_pin_output();
            xSemaphoreGiveFromISR(message_semaphore, &xHigherPriorityTaskWoken);
    }
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
    gpio_set_level(dht_pin,1);
    ets_delay_us(1000);
    /* Pull LOW for 20ms */
    gpio_set_level(dht_pin,0);
	// Should be at least 18
    ets_delay_us(20000);
    /* The pin will be set to PULLUP during the interruption setup */
}

/* Check the timings of the received signal edges and convert that to bits
 * 26-28us means FALSE
 * 70us means TRUE
 * Each bit follows a 50us LOW signal
 * */
static int dht_parse_bits_from_edges(uint8_t *bits) {
    int ret = DHT_OK;
    int i;
    uint8_t  offset = 0;
    int sync = 1;
    int start = 0;

    /* FIXME shitty hack to find the first relevant edge (~54µs)
     * Sometimes 2, sometimes 3 */
    for(i = 1; i < DHT_EDGES_PER_READ; i++) {
        uint64_t diff = edges[i].ts-edges[i-1].ts;
        if((diff>50) && (diff<60)) {
            start = i;
            break;
        }
    }

    for(i = start; i < DHT_EDGES_PER_READ; i++) {
        /* Compute the length of the pulse in µs */
        uint64_t diff = edges[i].ts-edges[i-1].ts;
        //printf("%d: %lld\n", i, diff);
        if((diff>40) && (diff<69)) {         // Per doc, 50µs
            // SYNC
            sync = 2;
        }
        else if((diff>15) && (diff<35)) {    // Per doc, 26-28µs
            // FALSE
            if(sync!=1) {
                ret = DHT_SYNC_ERROR;
                goto end;
            }
            offset++;
        }
        else if((diff>=69) && (diff < 80)) {  // Per doc, 70µs
            // TRUE
            bits[offset] = 1;
            offset++;
        } else {
            ret = DHT_TIMING_ERROR;
            goto end;
        }
        sync--;
    }
end:
    return ret;
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
/* FIXME Doesn't work the first time(s), for some reason */
int dht_get_data(void) {
    int ret;
    uint8_t bits[40] = {0x00};

    intr_start_time = 0;
    isr_count = 0;
    memset(edges, 0, DHT_EDGES_PER_READ*sizeof(struct {uint64_t ts; int value;}));

    /* Create semaphore released by the ISR on full message reception */
    message_semaphore = xSemaphoreCreateBinaryStatic(&message_semaphore_buffer);
    /* Wakeup the device */
    dht_send_start();
    /* Setup GPIO interrupt service */
    dht_setup_interrupt();

    /* Wait for the semaphore to be released */
    if( xSemaphoreTake(message_semaphore, (DHT_RESPONSE_TIMEOUT_US/1000) / portTICK_PERIOD_MS) != pdTRUE) {
            gpio_uninstall_isr_service();
            ret = DHT_TIMEOUT_ERROR;
            printf("Timeout, %d edges\n", isr_count);
            goto end;
    }

    /* Get a bitfield from the edges */
    ret = dht_parse_bits_from_edges(bits);

    /* Parse the bitfield into usable bytes */
    if(ret == DHT_OK) {
        uint8_t hum_int = dht_decode_byte(bits);
        uint8_t hum_dec = dht_decode_byte(&bits[8]);
        uint8_t temp_int = dht_decode_byte(&bits[16]);
        uint8_t temp_dec = dht_decode_byte(&bits[24]);
        uint8_t checksum = dht_decode_byte(&bits[32]);

        /* Verify checksum */
        if (((hum_int + hum_dec + temp_int + temp_dec) & 0xff) != checksum) {
            ret = DHT_CHECKSUM_ERROR;
        } else {
            /* We're good */
#if 1 // DHT22
            dht_humidity = ((hum_int<<8) | hum_dec)*100;
            dht_temp =     ((temp_int<<8)+temp_dec)*100;
#else // DHT11
            dht_humidity = (hum_int*1000)+hum_dec;
            dht_temp = (temp_int*1000)+temp_dec;
#endif
            ret = DHT_OK;
        }
    }
end:
    gpio_uninstall_isr_service();
    return ret;
}

