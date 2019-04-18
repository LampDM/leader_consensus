/* Simple example for I2C / BMP180 / Timer & Event Handling
 *
 * This sample code is in the public domain.
 */
#include "espressif/esp_common.h"
#include "esp/uart.h"

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

#include "i2c/i2c.h"

#include "ota-tftp.h"
#include "rboot-api.h"

#include "RF24/nRF24L01.h"
#include "RF24/RF24.h"


#define PCF_ADDRESS 0x38
#define gpio 2


#define NCS	15
#define CS_NRF	0

#define BUS	0

RF24 radio(2, 0);

//#define readBytes(slave_addr, t data, len, buf) i2c_slave_read(slave_addr, t data, buf, len)
// readBytes(MPU9250_ADDRESS, MPU9250_ACCEL_XOUT_H, 6, &rawData[0]);        // Read the six raw data registers into data array

void delay(int del)
{
	   vTaskDelay(del / portTICK_PERIOD_MS); // sleep 100ms  // Delay a while to let the device stabilize
}

void readBytes(uint8_t slave_addr, uint8_t cmd, uint8_t len, uint8_t *data)
{
	i2c_slave_read(BUS, slave_addr, &cmd, data, len);
}

uint8_t readByte(uint8_t slave_addr, uint8_t cmd)
{
	uint8_t data;

	readBytes(slave_addr, cmd, 1, &data);
	return(data);

}

bool byte_read(uint8_t slave_addr, uint8_t *buf, uint32_t len)
{
    bool success = false;
    do {
//        i2c_start(BUS);
//        if (!i2c_write(BUS, slave_addr << 1)) {
//            break;
//        }
//        i2c_write(BUS, data);
//        i2c_stop(BUS);
        i2c_start(BUS);
        if (!i2c_write(BUS, slave_addr << 1 | 1)) { // Slave address + read
            break;
        }
        while(len) {
            *buf = i2c_read(BUS, len == 1);
            buf++;
            len--;
        }
        success = true;
    } while(0);
    i2c_stop(BUS);
    if (!success) {
        printf("I2C: write error\n");
    }
    return success;
}

void writeByte(uint8_t slave_addr, uint8_t data)
{
	uint8_t ddata[2];

	ddata[0] = data;

	i2c_slave_write(BUS, slave_addr, NULL, ddata, 1);

}

void test_task1(void *pvParameters)
{
	uint8_t scl = 14, sda = 12;

	i2c_init(BUS, scl, sda, I2C_FREQ_400K);

	uint8_t lled = 0b11111110;

	// Main loop
	while (true){
			printf("test task running!\n");
			for (int i=0; i<4; i++) {
				writeByte(PCF_ADDRESS, lled);
				vTaskDelay(100 / portTICK_PERIOD_MS);

				lled <<= 1; lled |= 0x01;
			}
			lled = 0b11111110;

	}

}

void test_task2(void *pvParameters)
{
	uint8_t scl = 14, sda = 12;

	i2c_init(BUS, scl, sda, I2C_FREQ_400K);

	while (true){
		printf("secondary task listening!\n");
		vTaskDelay(1000 / portTICK_PERIOD_MS);

	}

}

// Setup HW
void user_setup(void)
{
    // Set UART Parameter
    uart_set_baud(0, 115200);

    // Give the UART some time to settle
    sdk_os_delay_us(500);
}

extern "C" void user_init(void); // one way
void user_init(void)
{

    user_setup();

    sdk_wifi_set_opmode(NULL_MODE);	// STATION_MODE

		xTaskCreate(test_task1,"test_task",512,0,2,NULL);
		xTaskCreate(test_task2,"test_task2",512,0,2,NULL);

}
