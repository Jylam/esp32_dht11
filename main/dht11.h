/* DHT11 temperature sensor library
 * (c) 2018 Jylam
 */
#ifndef DHT11_H_
#define DHT11_H_

#define DHT_TIMEOUT_ERROR  -1
#define DHT_SYNC_ERROR     -2
#define DHT_CHECKSUM_ERROR -3
#define DHT_OK  1

void dht_set_pin(int pin);
int  dht_get_data(void);
int  dht_get_temperature(void);
int  dht_get_humidity(void);


#endif

