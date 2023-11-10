#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <wiringx.h>

// Use 6.4MHz to allow for bit banging of the WS2812 signal to be "close enough"
#define SPISPEED 6400000
// We use SPI bus 0 on the Milk V Duo
#define SPIBUS 0
#define NUMLEDS 5
#define GRB 3
#define GRBW 4
#define ONE 0xFC
#define ZERO 0x80
#define RESETLENGTH 25
#define RESET 0x00

/*
*  Bit bang WS2812 800KHz signal using SPI hardware
*  
*/

int bufferSize = 0;

int msleep(unsigned int tms) {
  return usleep(tms * 1000);
}

int8_t initSPI(){
	if(wiringXSetup("duo", NULL) == -1) {
        	wiringXGC();
		printf("Could not initialize wiringX\n");
        	return -1;
    	}

	if(wiringXSPISetup(SPIBUS, SPISPEED) < 0){
		printf("Failed to initialize SPI bus\n");
		return -1;
	}
}

void sendData(uint8_t* data){

	uint8_t *spiData = malloc((sizeof(uint8_t) * bufferSize) * (sizeof(uint8_t) * 8));

	for(uint8_t i=0; i < bufferSize; i++){
		for(uint8_t j=0; j < 8; j++){
			*(spiData + i*8 + j) = (data[i] & (0x80 >> j)) ? ONE : ZERO;
		}
	}

	wiringXSPIDataRW(SPIBUS, spiData, bufferSize*8);
	free(spiData);
}

uint8_t* initDataBuffer(uint8_t type){

	uint8_t *data;
	bufferSize = ((type * NUMLEDS) + RESETLENGTH);

	data = malloc(sizeof(uint8_t) * bufferSize);
	memset(data, 0, bufferSize);

	return data;

}

void resetBuffer(uint8_t* data){
	memset(data, 0, bufferSize);
}

void setLED(uint8_t* data, uint8_t type, uint8_t red, uint8_t green, uint8_t blue, uint8_t white, uint8_t id){

	switch(type){

		case(GRB):

			data[id*type] = green;
			data[id*type + 1] = red;
			data[id*type + 2] = blue;
			break;

		default:
			printf("Unsupported LED format\n");
			exit(-1);

	}

}

int main() {

	// Initialize SPI and data
	if(initSPI() < 0){ exit(-1); }
	uint8_t *data = initDataBuffer(GRB);

	// Wipe red
	for(uint8_t i=0; i < NUMLEDS; i++){
		setLED(data, GRB, 255, 0, 0, 0, i);
		sendData(data);
		msleep(250);
	}

	// Wipe green
	for(uint8_t i=0; i < NUMLEDS; i++){
		setLED(data, GRB, 0, 255, 0, 0, i);
		sendData(data);
		msleep(250);
	}

	// Wipe blue
	for(uint8_t i=0; i < NUMLEDS; i++){
		setLED(data, GRB, 0, 0, 255, 0, i);
		sendData(data);
		msleep(250);
	}

	resetBuffer(data);

	// Flash alternating LEDs white
	for(uint8_t i=0; i < 32; i++){
		resetBuffer(data);
		for(uint8_t j=0; j < NUMLEDS; j++){
			if(i%2 == 0 && j%2 == 0) setLED(data, GRB, 255, 255, 255, 0, j);
			else if(i%2 == 1 && j%2 == 1) setLED(data, GRB, 255, 255, 255, 0, j);
		}
		sendData(data);
		msleep(100);
	}

	resetBuffer(data);

	uint8_t red, green, blue;

	for(uint8_t i=0; i < NUMLEDS; i++){
		red = (rand() % 16) * 16;
		green = (rand() % 16) * 16;
		blue = (rand() % 16) * 16;

		setLED(data, GRB, red, green, blue, 0, i);
	}

	for(uint8_t hue=0; hue < 16; hue++){
		for(uint8_t i=0; i < NUMLEDS; i++){
			setLED(data, GRB, data[i*GRB] + 16, data[i*GRB+1] + 16, data[i*GRB+2] + 16, 0, i);
		}
		sendData(data);
		msleep(100);
	}

	// Turn off at end
	resetBuffer(data);
	sendData(data);

	free(data);

	exit(0);

}
