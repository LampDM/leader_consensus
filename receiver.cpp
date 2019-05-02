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
bool hastoSend = false;
bool hastoVote = false;


TaskHandle_t xHandle = NULL;
void reset_election_timer();
bool seenDevice(char d);
void removeInactiveDevs();
void addDevice(char d);
void printDevices();
void changeRole(int r);
void sendMsg(char* addr,char* type, char* payload);
int votes = 0;

// Device data table
char devs[10] = {'X','X','X','X','X','X','X','X','X','X'};
signed int devsSeen[10] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
int devc = 1;

// Raft variables
bool isLeader = false;
bool isFollower = true;
bool isCandidate = false;
int termCount = 0;
int sendInterval = 5000;

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
		vTaskDelay(pdMS_TO_TICKS(sendInterval+r*100));
		votes = 0;
		hastoSend=true;
		termCount++;
	}

}

void sendMsg(char* addr,char* type, char* payload){
	reset_radio();
	radio.openWritingPipe(address);
	radio.stopListening();
	//Send message here
	strcpy((tx_buffer)+0,my_addr);
	strcpy((tx_buffer)+1,addr);
	strcpy((tx_buffer)+2,type);
	strcpy((tx_buffer)+3,payload);
	radio.write(&tx_buffer,sizeof(tx_buffer));
	radio.openReadingPipe(1, address);
}

void LR_task (void *pvParameters){
	printf("LR_task init\n");
	radio.openReadingPipe(1, address);
	while(1){

		if( (devc/2)<votes ){
			//Become the leader if majority is recieved
			changeRole(2);
			votes=0;
			sendMsg("F","H","ppppp");
		}

		radio.startListening();
		if (radio.available()) {
			printf("before print devices\n");
			radio.read(&rx_data, sizeof(rx_data));

					char* sender = substr(rx_data,0,1);

					// Checks if a new device entered the network
					if(! seenDevice(sender[0])){
						addDevice((int)(sender[0]));
					}
					removeInactiveDevs();
					//TODO fix bug where if from 2 machines we go from 1 the 1 is a forever candidate
					//TODO implement anti-gridlock mechanism if all nodes become candidates
					//TODO fix bouncing leader bug from 1 node to another and back again
					//TODO why is printDevices() not working unless node is follower,
					//because print only goes on if the device recieves some signal
					//make a task for device housekeeping?
					//For debugging purposes
					printDevices();

					if (relevantData(rx_data)){
						printf("Received message: ");
						printf(rx_data);
						printf("\n");
						char* type = substr(rx_data,2,3);

						switch ((int)type[0]){

							case ((int)'H'):
							reset_election_timer();
							printf("Heartbeat recieved!\n");
							//Become follower
							changeRole(0);
							votes = 0;
							break;

							case ((int)'V'):
								if(isCandidate){
									votes++;
									printf("Recieved vote!\n");
								}
							break;
							case ((int)'E'):
								//Do direct voting here
								if (isFollower){
									//TODO Decide who is a suitable candidate
										sendMsg(sender,"V","PPPPP");
								}else
								if(isLeader){
									sendMsg(sender,"H","PPPPP");
								}
							break;

							default:
								printf("unknown message type!\n");
								break;
						}

					}

		}else{
				if (hastoSend){

					hastoSend = false;

					if(isLeader){
						sendMsg("F","H","ppppp");
					}else
					if(isFollower){
						//Become a candidate
						changeRole(1);
						votes++;
						sendMsg("F","E","ppppp");
					}else
					if(isCandidate){
						sendMsg("F","E","ppppp");
						votes=0;
						votes++;
					}
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
	int k;
	int ld = len(devs);
	for(k = 0;k<ld;k++){
		if ((int)d == devs[k]){
			devsSeen[k]=termCount;
			return true;
		}
	}
	return false;
}

void removeInactiveDevs(){
	int k;
	int ld = len(devs);
	for(k = 0;k<ld;k++){
		if (devsSeen[k]==-1){continue;}
		if(devsSeen[k]<termCount-5){
			devs[k]='X';
			devsSeen[k]=-1;
			devc--;
		}
	}
}

void addDevice(char d){
	int k;
	int ld = len(devs);
	for(k = 0;k<ld;k++){
		if((int)devs[k] == (int)'X'){
			devs[k]=d;
			devsSeen[k]=termCount;
			devc++;
			break;
		}
	}
}

void printDevices(){
	printf("devc %d\n",devc);
	printf("Device table:\n");
	int k;
	int ld = len(devs);
	for(k = 0;k<ld;k++){
		if(devsSeen[k]==-1){continue;}
		printf("dev %c lastseen %d \n",devs[k],devsSeen[k]);
	}
}

void changeRole(int r){
	isFollower=false;
	isCandidate=false;
	isLeader=false;
	sendInterval=5000;
	write_byte_pcf(0xff);
	switch(r){
		case 0:
		isFollower=true;
		write_byte_pcf(0b11110111);
		break;

		case 1:
		isCandidate=true;
		write_byte_pcf(0b11110011);
		break;

		case 2:
		isLeader=true;
		sendInterval=2800;
		write_byte_pcf(0b11110001);
		break;
	}
}

extern "C" void user_init(void) {

	setup_nrf();
	xTaskCreate(LR_task,"Listen and react task",1000, NULL ,2,NULL);
	xTaskCreate(election_timer, "Election timer",1000,NULL,3,&xHandle);
	reset_election_timer();


}
