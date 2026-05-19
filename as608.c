#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#define BAUD2 57600
#define UBRR_VAL ((F_CPU / (16UL * BAUD2)) - 1)

#include "as608.h"
#include <avr/io.h>
#include <util/delay.h>

//USART Functions

static void USART0_Init(void) {
	// Set baud rate for USART0
	UBRR0H = (unsigned char)(UBRR_VAL >> 8);
	UBRR0L = (unsigned char)UBRR_VAL;

	// Enable receiver and transmitter on USART0
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);

	// Set frame format: 8 data bits, 1 stop bit, no parity
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

static void USART0_Transmit(unsigned char data) {
	// Wait for empty transmit buffer
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = data;
}

static unsigned char USART0_Receive(void) {
	// Wait for data to be received
	while (!(UCSR0A & (1 << RXC0)));
	return UDR0;
}

//AS608 Function to send command, used by all of the below functions 

static uint8_t AS608_ExecuteCmd(uint8_t *cmd, uint8_t cmd_len, uint8_t resp_len) {
	// Send command via USART0
	for (uint8_t i = 0; i < cmd_len; i++) {
		USART0_Transmit(cmd[i]);
	}

	// Read response via USART0
	uint8_t response[16];
	for (uint8_t i = 0; i < resp_len; i++) {
		response[i] = USART0_Receive();
	}

	return response[9];
}

//AS608 Helper Functions, generate image, verify, delete, add etc

//The AS608 requires a strict structure that goes as follows
//Header [0:1] always 0xEF01
//Address [2:5] always 0xFFFFFFFF
//Package ID [6] For our purpose always 0x01 indicating a command, but 0x02 indicates data
//Length [7:8] length of package ID, data and checksum so value varies
//Contents [9:N] this value and length varies for example the store function its 0x06 whereas for the search its 0x08
//Checksum [N+1-N+2] This is the last 2 bytes and it varies based on the sum of the package ID, length and contents bytes

void AS608_Init(void) {
	USART0_Init();
	_delay_ms(500);
}
//function to generate an image, basically takes a picture of whatever is on the glass.  
uint8_t AS608_GenImg(void) {
    uint8_t cmd[12] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, 0x01, 0x00, 0x05};
    return (AS608_ExecuteCmd(cmd, 12, 12) == 0x00) ? 1 : 0;
}

uint8_t AS608_Img2Tz(void) {
    uint8_t cmd[13] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x04, 0x02, 0x01, 0x00, 0x08};
    return (AS608_ExecuteCmd(cmd, 13, 12) == 0x00) ? 1 : 0;
}

uint8_t AS608_Img2Tz2(void) {
    uint8_t cmd[13] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x04, 0x02, 0x02, 0x00, 0x09};
    return (AS608_ExecuteCmd(cmd, 13, 12) == 0x00) ? 1 : 0;
}

uint8_t AS608_Search(void) {
    uint8_t cmd[17] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x08, 0x04, 0x01, 0x00, 0x00, 0x01, 0x2C, 0x00, 0x3B};
    return (AS608_ExecuteCmd(cmd, 17, 16) == 0x00) ? 1 : 0;
}

uint8_t AS608_RegModel(void) {
    uint8_t cmd[12] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, 0x05, 0x00, 0x09};
    return (AS608_ExecuteCmd(cmd, 12, 12) == 0x00) ? 1 : 0;
}

uint8_t AS608_Store(uint16_t id) {
    uint8_t cmd[15];
    cmd[0] = 0xEF; cmd[1] = 0x01;
    cmd[2] = 0xFF; cmd[3] = 0xFF;
    cmd[4] = 0xFF; cmd[5] = 0xFF;
    cmd[6] = 0x01;
    cmd[7] = 0x00; cmd[8] = 0x06;
    cmd[9] = 0x06;
    cmd[10] = 0x01;
    cmd[11] = (uint8_t)(id >> 8);
    cmd[12] = (uint8_t)(id & 0xFF);

    uint16_t sum = cmd[6] + cmd[7] + cmd[8] + cmd[9] + cmd[10] + cmd[11] + cmd[12];
    cmd[13] = (uint8_t)(sum >> 8);
    cmd[14] = (uint8_t)(sum & 0xFF);

    return (AS608_ExecuteCmd(cmd, 15, 12) == 0x00) ? 1 : 0;
}

//master AS608 Functions for basic operation and programming

uint8_t AS608_ScanAndVerify(void) {
    if (!AS608_GenImg()) return 0; // No finger, or bad read
    if (!AS608_Img2Tz()) return 0; // Failed to convert to template
    if (!AS608_Search()) return 0; // Fingerprint not in database
    return 1; //return 1 means success
}

uint8_t AS608_EnrollProcess(uint16_t new_id) {
    //Wait for first press
    while (AS608_GenImg() == 0) {
        _delay_ms(100);
    }

    if (AS608_Img2Tz() == 0) return 0;

    //Wait for release
    while (AS608_GenImg() == 1) {
        _delay_ms(100);
    }

    _delay_ms(1000); 

    //Wait for second press
    while (AS608_GenImg() == 0) {
         _delay_ms(100);
    }

    if (AS608_Img2Tz2() == 0) return 0;

    //Combine and Store
    if (AS608_RegModel() == 0) return 0;
    if (AS608_Store(new_id) == 0) return 0;

    return 1; //enroll success
}

uint8_t AS608_EmptyDatabase(void) {
	uint8_t cmd[12] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, 0x0D, 0x00, 0x11};

	// Confirmation code 0x00 means the database was successfully wiped
	return (AS608_ExecuteCmd(cmd, 12, 12) == 0x00) ? 1 : 0;
}

uint8_t AS608_DeleteID(uint16_t id) {
	uint8_t cmd[16];

	cmd[0] = 0xEF; cmd[1] = 0x01;             
	cmd[2] = 0xFF; cmd[3] = 0xFF;             
	cmd[4] = 0xFF; cmd[5] = 0xFF;
	cmd[6] = 0x01;                            
	cmd[7] = 0x00; cmd[8] = 0x07;            
	cmd[9] = 0x0C;                            
	cmd[10] = (uint8_t)(id >> 8);             // Target ID to delete, High Byte
	cmd[11] = (uint8_t)(id & 0xFF);           // Target ID to delete, Low Byte
	cmd[12] = 0x00;                           
	cmd[13] = 0x01;                           


	// Calculate Checksum: Sum of Packet ID + Length + Instruction + Target ID + Number to delete
	uint16_t sum = cmd[6] + cmd[7] + cmd[8] + cmd[9] + cmd[10] + cmd[11] + cmd[12] + cmd[13];
	cmd[14] = (uint8_t)(sum >> 8);            
	cmd[15] = (uint8_t)(sum & 0xFF);          


	// Confirmation code 0x00 means the specific ID was successfully deleted
	return (AS608_ExecuteCmd(cmd, 16, 12) == 0x00) ? 1 : 0;
}