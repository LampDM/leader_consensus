#include "nrf24.h"

#include <espressif/esp_sta.h>
#include <espressif/esp_wifi.h>

extern "C" {
#include "paho_mqtt_c/MQTTESP8266.h"
#include "paho_mqtt_c/MQTTClient.h"
}

#include <semphr.h>

#define rx_size		100
static char rx_data[rx_size];

#define MQTT_HOST	"test.mosquitto.org"
#define MQTT_PORT	1883
#define MQTT_TOPIC	"/bsovaje"

#define MQTT_USER NULL
#define MQTT_PASS NULL

#define PUB_MSG_LEN 16

#define tx_size		6
static char tx_buffer[tx_size] = "hello" ;
static int c = 0;

char int_to_char(int i){

	return i+48;

}

// transmit data
void transmit_nrf24() {

	// turn on led1
	write_byte_pcf(led1);

	radio.powerUp();
	radio.stopListening();
	//radio.write(&tx_buffer, sizeof(tx_buffer));
	//tx_buffer[0]=c;
	tx_buffer[0]=int_to_char(c);
	radio.write(&tx_buffer,sizeof(tx_buffer));
	c++;
	radio.powerDown();

	// turn off leds
	vTaskDelay(pdMS_TO_TICKS(300));
	write_byte_pcf(0xff);
}

static const char * get_my_id(void) {
	// Use MAC address for Station as unique ID
	static char my_id[13];
	static bool my_id_done = false;
	int8_t i;
	uint8_t x;
	if (my_id_done)
		return my_id;
	if (!sdk_wifi_get_macaddr(STATION_IF, (uint8_t *) my_id))
		return NULL;
	for (i = 5; i >= 0; --i) {
		x = my_id[i] & 0x0F;
		if (x > 9)
			x += 7;
		my_id[i * 2 + 1] = x + '0';
		x = my_id[i] >> 4;
		if (x > 9)
			x += 7;
		my_id[i * 2] = x + '0';
	}
	my_id[12] = '\0';
	my_id_done = true;
	return my_id;
}



void send_task(void *pvParameters) {
	radio.openWritingPipe(address);
	radio.powerDown();
	while (1){
		printf("sending a shitty message\n");
		transmit_nrf24();
		vTaskDelay(pdMS_TO_TICKS(15000));
	}
}

// listen to the radio
void listen_task(void *pvParameters) {

	radio.openReadingPipe(1, address);
	radio.startListening();

	static char msg[PUB_MSG_LEN];
	static int count = 0;

	while (1) {

		if (radio.available()) {
			radio.read(&rx_data, sizeof(rx_data));
			printf("Received message: %s\n", rx_data);

			// turn on led1
			write_byte_pcf(led1);
			vTaskDelay(pdMS_TO_TICKS(200));
			write_byte_pcf(0xff);
		}

		// button1 is pressed
		if ((read_byte_pcf() & button1) == 0) {
			// turn off leds
			write_byte_pcf(0xff);
		}

		// sleep for 200 ms
		radio.powerDown();
		vTaskDelay(pdMS_TO_TICKS(200));
		radio.powerUp();
	}
}

extern "C" void user_init(void) {


	setup_nrf();

	xTaskCreate(listen_task, "Radio listen task", 1000, NULL, 3, NULL);
	xTaskCreate(send_task,"Sending task on radio",1000,NULL,3,NULL);



}
