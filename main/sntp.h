#ifndef SNTP_H_
#define SNTP_H_

#include "freertos/event_groups.h"

bool initialize_sntp(void);
extern EventGroupHandle_t sntp_event_group;
extern time_t now;

#define SNTP_SUCCESS_BIT  BIT0
#define SNTP_FAILURE_BIT  BIT1


#endif
