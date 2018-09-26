#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_http_client.h"
#include "lwip/apps/sntp.h"

#include "wifi.h"

void initialize_sntp(void)
{
    char strftime_buf[64];
    printf("Initializing SNTP\n");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "fr.pool.ntp.org");
    sntp_init();


    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;

    int got_network = xEventGroupWaitBits(
            wifi_event_group ,
            CONNECTED_BIT,
            pdFALSE,        // Don't clear bit after waiting
            pdFALSE,
            100000 / portTICK_PERIOD_MS);

    if(got_network) {
        int retry_count = 10;
        while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
            printf("Waiting for system time to be set... (%d/%d)\n", retry, retry_count);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            time(&now);
            localtime_r(&now, &timeinfo);
        }

        setenv("TZ", "GMT+2/-2,M10.5.0/-1", 1);
        tzset();
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        printf("The current date/time is: %s\n", strftime_buf);
    }

    sntp_stop();
}
