#include "nrf24.h"
#include <espressif/esp_sta.h>
#include <espressif/esp_wifi.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
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

#define tx_size		32
static char tx_buffer[tx_size] = "     HELLO" ;
static bool hastoSend = false;

bool isLeader = false;
bool isFollower = true;
bool isCandidate = false;

char int_to_char(int i){
	return i+48;
}


static char * get_my_id(void) {
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

	const char* arr = get_my_id();
	const char my_addr[] = {arr[8],arr[9],arr[10],arr[11]};



// transmit data
void transmit_nrf24() {
	//TODO payload and dest address handling
	strcpy(tx_buffer,my_addr);
	radio.write(&tx_buffer,sizeof(tx_buffer));

}


void reset_radio(){
	//Shut down and reset radio here
	radio.stopListening();
	radio.powerDown();
	gpio_enable(SCL, GPIO_OUTPUT);
	gpio_enable(CS_NRF, GPIO_OUTPUT);
	// radio configuration
	radio.begin();
	radio.setChannel(channel);
	radio.setAutoAck(false);
	radio.powerUp();
}

void election_timer(void *pvParameters){
	while(1){
		int r = rand() % 20;
		vTaskDelay(pdMS_TO_TICKS(5000+r*100));
			hastoSend=true;
	}

}

void LR_task (void *pvParameters){

	printf("some init\n");

	radio.openReadingPipe(1, address);
	while(1){



		radio.startListening();
		if (radio.available()) {

			radio.read(&rx_data, sizeof(rx_data));

					printf("Received message: %c\n", rx_data[0]);
					//hastoSend = true;
					//printf("%d\n",rx_data[0]);
					printf(rx_data);
					printf("\n");
					//int someint = *((int*)pvParameters);

					// turn on led1
					write_byte_pcf(led1);
					vTaskDelay(pdMS_TO_TICKS(200));
					write_byte_pcf(0xff);


		}else{
				if (hastoSend){
					reset_radio();
					radio.openWritingPipe(address);
					radio.stopListening();
					transmit_nrf24();
					radio.printDetails();
					hastoSend = false;
					radio.openReadingPipe(1, address);

				}
		}

	}

}

extern "C" void user_init(void) {

	setup_nrf();
	xTaskCreate(LR_task,"Listen and react task",1000, NULL ,2,NULL);
	xTaskCreate(election_timer, "Election timer",1000,NULL,3,NULL);

}
