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
static char tx_buffer[tx_size] = "eeeeeeeeeeeeeeeeee" ;
static bool hastoSend = false;


TaskHandle_t xHandle = NULL;
void reset_election_timer();
bool seenDevice(char d);

int devc = 0;
char devs[10] = {0,0,0,0,0,0,0,0,0,0};

// Raft variables
bool isLeader = false;
bool isFollower = true;
bool isCandidate = false;

int electionCount = 0;

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



	int len(const char *arr){
		return sizeof(arr)/sizeof(arr[0]);
	}

	char* substr(const char *src, int m, int n)
	{
	        int len = n - m;
	        char *dest = (char*)malloc(sizeof(char) * (len + 1));
	        int i;
	        for (i = m; i < n && (*src != '\0'); i++)
	        {
	                *dest = *(src + i);
	                dest++;
	        }
	        *dest = '\0';
	        return dest - len;
	}

	char* arr = get_my_id();
	char* my_addr = {substr(arr,11,12)};

bool relevantData(char* data){
	//Check if dest address is F(broadcast) or my_addr
	return (strcmp(substr(data,1,2),"F") == 0 or strcmp(substr(data,1,2),my_addr) == 0);
}

// transmit data
void transmit_nrf24() {
	//TODO payload handling
	strcpy((tx_buffer)+0,my_addr); //Sender address
	strcpy((tx_buffer)+1,"F"); //Destination address
	//H means heartbeat timer reset and data update
	//V is for voting
	strcpy((tx_buffer)+2,"VPPPPP"); //Payload is limited to 6 extra ints

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
	//TODO Figure out how to reset timer on command
	//TODO Implement first real election
	while(1){
		int r = rand() % 20;
		vTaskDelay(pdMS_TO_TICKS(5000+r*100));
		hastoSend=true;
		electionCount++;
		isCandidate = true;
	}

}

void LR_task (void *pvParameters){
	printf("LR_task init\n");
	radio.openReadingPipe(1, address);
	while(1){
		radio.startListening();
		if (radio.available()) {

			radio.read(&rx_data, sizeof(rx_data));

					char* sender = substr(rx_data,0,1);
					//TODO Check if device is a new device not yet seen
					if(seenDevice(sender[0])){
						printf("device already seen before\n");
						printf("current devices %d\n",devc);


					}else{
						devs[devc]=(int)sender[0];
						devc++;
					}

					if (relevantData(rx_data)){
						printf("Received message: ");
						printf(rx_data);
						printf("\n");

						char* type = substr(rx_data,2,3);

						//TODO Check different kinds of messages
						switch ((int)type[0]){
							case ((int)'H'):
							reset_election_timer();
							//TODO Update data + checks if it's sent by the leader
							break;
							case ((int)'V'):
							//TODO handle incoming vote
							break;
							default:
								printf("unknown message type!\n");
								break;
						}

						// turn on led1
						write_byte_pcf(led1);
						vTaskDelay(pdMS_TO_TICKS(200));
						write_byte_pcf(0xff);
					}

		}else{
				if (hastoSend){
					reset_radio();
					radio.openWritingPipe(address);
					radio.stopListening();
					transmit_nrf24();
					hastoSend = false;
					radio.openReadingPipe(1, address);

				}
		}

	}

}
void reset_election_timer(){
	printf("Resetting election timer\n");
	if (xHandle != NULL){
		vTaskDelete(xHandle);
		xTaskCreate(election_timer, "Election timer",1000,NULL,3,&xHandle);
	}
}

bool seenDevice(char d){
	int k; //TODO fix bug
	int ld = len(devs);
	printf("%d length of devs\n",ld);
	for(k = 0;k++;k<ld){
		printf("%d == %d\n",(int)d,devs[k]);
		if ((int)d == devs[k]){
			return true;
		}
	}
	return false;
}

extern "C" void user_init(void) {

	setup_nrf();
	xTaskCreate(LR_task,"Listen and react task",1000, NULL ,2,NULL);
	xTaskCreate(election_timer, "Election timer",1000,NULL,3,&xHandle);
	reset_election_timer();


}
