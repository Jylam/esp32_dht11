#ifndef WIFI_H_
#define WIFI_H_

#include "freertos/event_groups.h"

int wifi_init(void);
int wifi_deinit(void);

extern EventGroupHandle_t wifi_event_group;
#define CONNECTION_NEEDED_BIT  BIT0
#define CONNECTED_BIT  BIT1


#endif
