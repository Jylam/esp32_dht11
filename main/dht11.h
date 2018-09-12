/* DHT11 temperature sensor library
 * (c) 2018 Jylam
 */
#ifndef DHT11_H_
#define DHT11_H_

#define DHT_TIMEOUT_ERROR -2
#define DHT_CHECKSUM_ERROR -1
#define DHT_OKAY  0

// Public
void dht_set_pin(int pin);
int dht_get_data(void);

// Private
int dht_send_start(void);
void dht_handle_error(int response);

#endif

