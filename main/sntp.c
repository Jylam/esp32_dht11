#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "lwip/apps/sntp.h"

#include "sntp.h"
#include "wifi.h"


EventGroupHandle_t sntp_event_group;
time_t now = 0;

bool initialize_sntp(void)
{
    bool ret = false;
    sntp_event_group = xEventGroupCreate();
    xEventGroupClearBits(sntp_event_group, SNTP_SUCCESS_BIT);

    printf("Initializing SNTP\n");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "fr.pool.ntp.org");
    sntp_init();


    struct tm timeinfo = { 0 };
    int retry = 0;

    int got_network = xEventGroupWaitBits(
            wifi_event_group ,
            CONNECTED_BIT,
            pdFALSE,        // Don't clear bit after waiting
            pdTRUE,
            5000 / portTICK_PERIOD_MS);

    if(got_network & CONNECTED_BIT) {
        int retry_count = 20;
        while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
            vTaskDelay(500 / portTICK_PERIOD_MS);
            time(&now);
            localtime_r(&now, &timeinfo);
        }
        if(retry< retry_count) {
            printf("Got NTP\n");

            xEventGroupSetBits(sntp_event_group, SNTP_SUCCESS_BIT);
            ret = true;
        } else {
            printf("Can't get NTP\n");
            xEventGroupSetBits(sntp_event_group, SNTP_FAILURE_BIT);
        }

    } else {
        printf("Can't get network for NTP\n");
        xEventGroupSetBits(sntp_event_group, SNTP_FAILURE_BIT);
        ret = false;
    }
    return ret;
}
