#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#include <avr/io.h>
#include <util/delay.h>
#include <u8g2.h>
#include <u8x8_avr.h>
#include "as608.h"
#include "lock_servo.h"
#include <avr/eeprom.h>
#include <string.h>
#include <stdlib.h>

#define MAX_PINS 3
char EEMEM stored_pins[5][5] = {
	"0000", "0000", "0000", "0000", "0000"
};

char pin_buffer[5] = {0};
uint8_t pin_index = 0;
u8g2_t u8g2;

void clear_pin_buffer() {
	for (uint8_t i = 0; i < 5; i++) pin_buffer[i] = 0;
	pin_index = 0;
}


uint8_t check_pin()
{
	char saved[5];

	for (uint8_t i = 0; i < MAX_PINS; i++)
	{
		// Read PIN slot i
		eeprom_read_block(saved, stored_pins[i], 5);
		// Compare with entered PIN
		if (strcmp(saved, pin_buffer) == 0)
		return 1;
	}

	return 0;
}


void save_pin(char *newpin)
{
	char saved[5];

	for (uint8_t i = 0; i < MAX_PINS; i++)
	{
		eeprom_read_block(saved, stored_pins[i], 5);

		if (strcmp(saved, "AAAA") == 0)   // empty slot
		{
			eeprom_write_block(newpin, stored_pins[i], 5);
			return;
		}
	}
}

void reset_all_pins(void)
{
	char blank[5] = "AAAA";

	for (uint8_t i = 0; i < MAX_PINS; i++)
	eeprom_write_block(blank, stored_pins[i], 5);
}

void delete_pin_slot(uint8_t slot)
{
	char blank[5] = "AAAA";
	eeprom_write_block(blank, stored_pins[slot], 5);
}


// ---------------- KEYPAD SETUP ----------------
// Columns: PB1, PB0, PD7, PD6
// Rows:    PD5, PD4, PB3, PB2

void keypad_init(void)
{
	// ROWS as outputs: PD5, PD4, PD3, PD2
	DDRD |= (1<<PD5) | (1<<PD4) | (1<<PD3) | (1<<PD2);

	// COLUMNS as inputs with pull-ups: PB1, PB0, PD7, PD6
	DDRB &= ~((1<<PB1) | (1<<PB0));
	DDRD &= ~((1<<PD7) | (1<<PD6));

	PORTB |= (1<<PB1) | (1<<PB0);   // enable pull-ups
	PORTD |= (1<<PD7) | (1<<PD6);

	// Set all rows HIGH (inactive)
	PORTD |= (1<<PD5) | (1<<PD4) | (1<<PD3) | (1<<PD2);
}



char keypad_getkey(void)
{
	const char keymap[4][4] = {
		{'1','2','3','A'},   // Row 0  (PD5)
		{'4','5','6','B'},   // Row 1  (PD4)
		{'7','8','9','C'},   // Row 2  (PD3)
		{'*','0','#','D'}    // Row 3  (PD2)
	};

	// Row pins in correct order
	uint8_t row_bits[4] = { PD5, PD4, PD3, PD2 };

	for (uint8_t row = 0; row < 4; row++)
	{
		// Set all rows HIGH
		PORTD |= (1<<PD5) | (1<<PD4) | (1<<PD3) | (1<<PD2);

		// Drive current row LOW
		PORTD &= ~(1 << row_bits[row]);
		_delay_us(5);

		// Columns in correct order:
		// C1 = PB1, C2 = PB0, C3 = PD7, C4 = PD6
		uint8_t c1 = !(PINB & (1<<PB1));
		uint8_t c2 = !(PINB & (1<<PB0));
		uint8_t c3 = !(PIND & (1<<PD7));
		uint8_t c4 = !(PIND & (1<<PD6));

		if (c1 || c2 || c3 || c4)
		{
			_delay_ms(20); // debounce

			if (c1) return keymap[row][0];
			if (c2) return keymap[row][1];
			if (c3) return keymap[row][2];
			if (c4) return keymap[row][3];
		}
	}
	return 0;
}

char get_key_once(void)
{
	char key = 0;
	char k = keypad_getkey();

	if (k != 0) {
		key = k;                     // latch the key
		while (keypad_getkey() != 0) // wait for release
		_delay_ms(5);

		_delay_ms(50);               // debounce
	}

	return key;
}



// U8G2 CALLBACK
// SOFTWARE I2C
uint8_t u8x8_gpio_and_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
	switch(msg)
	{
		case U8X8_MSG_GPIO_AND_DELAY_INIT:
		keypad_init();
		DDRC |= (1<<PINC4) | (1<<PINC5); // SDA/SCL outputs
		break;

		case U8X8_MSG_GPIO_I2C_CLOCK:   // SCL
		if (arg_int) PORTC |= (1<<PINC5);
		else         PORTC &= ~(1<<PINC5);
		break;

		case U8X8_MSG_GPIO_I2C_DATA:    // SDA
		if (arg_int) PORTC |= (1<<PINC4);
		else         PORTC &= ~(1<<PINC4);
		break;

		case U8X8_MSG_DELAY_MILLI:
		while(arg_int--) _delay_ms(1);
		break;

		case U8X8_MSG_DELAY_10MICRO:
		while(arg_int--) _delay_us(10);
		break;

		default:
		return 0;
	}
	return 1;
}

// STATE MACHINE
typedef enum {
	
	STATE_MAIN_MENU,			//state for main menu, asks user for operating or programming mode
	STATE_PROGRAMMING_MENU,		//state for programming mode, stays in state until pin is entered or fingerprint is scanned
	STATE_OPERATING_MODE,		//state for operating mode, allows user to add and remove stored fingerprints or pins
	STATE_PIN_SUCCESS,
	STATE_PIN_FAIL,
	STATE_REMOVE_PIN_MENU
} SystemState;

SystemState state = STATE_MAIN_MENU;

// MENU DATA
const char *main_menu_items[] = {
	"Operating Mode",
	"Programming Mode"
};
const uint8_t MAIN_MENU_COUNT = 2;

uint8_t selected = 0;
uint8_t top = 0;

// DRAW MAIN MENU
void draw_main_menu(void)
{
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);

	u8g2_DrawStr(&u8g2, 0, 12, "Select Mode:");

	const uint8_t row_h = 12;
	const uint8_t first_y = 28;

	for (uint8_t i = 0; i < 4; i++)
	{
		uint8_t idx = top + i;
		if (idx >= MAIN_MENU_COUNT) break;

		uint8_t y = first_y + i * row_h;

		if (idx == selected)
		{
			u8g2_DrawBox(&u8g2, 0, y - 10, 128, row_h);
			u8g2_SetDrawColor(&u8g2, 0);
			u8g2_DrawStr(&u8g2, 10, y, main_menu_items[idx]);
			u8g2_DrawStr(&u8g2, 2, y, ">");
			u8g2_SetDrawColor(&u8g2, 1);
		}
		else
		{
			u8g2_DrawStr(&u8g2, 10, y, main_menu_items[idx]);
		}
	}

	u8g2_SendBuffer(&u8g2);
}

// PROGRAMMING MODE SCREEN
void draw_programming_menu(void)
{
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);

	u8g2_DrawStr(&u8g2, 0, 12, "Programming Mode");
	u8g2_DrawStr(&u8g2, 0, 28, "A: Add Pin");
	u8g2_DrawStr(&u8g2, 0, 40, "B: Remove Pin");
	u8g2_DrawStr(&u8g2, 0, 52, "C: Add Fingerprint");
	u8g2_DrawStr(&u8g2, 0, 64, "D: Remove Fingerprint");

	u8g2_SendBuffer(&u8g2);
}

// OPERATING MODE SCREEN
void draw_operating_mode(void)
{
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);

	u8g2_DrawStr(&u8g2, 0, 12, "Operating Mode");
	u8g2_DrawStr(&u8g2, 0, 28, "Enter PIN:");

	char display[6] = "____";
	for (uint8_t i = 0; i < pin_index; i++)
	display[i] = pin_buffer[i];

	u8g2_DrawStr(&u8g2, 0, 40, display);
	u8g2_DrawStr(&u8g2, 0, 56, "or Scan Fingerprint");

	u8g2_SendBuffer(&u8g2);
}

void draw_pin_success(void)
{
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
	u8g2_DrawStr(&u8g2, 0, 32, "PIN ACCEPTED");
	u8g2_SendBuffer(&u8g2);
}

void draw_pin_fail(void)
{
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
	u8g2_DrawStr(&u8g2, 0, 32, "PIN INCORRECT");
	u8g2_SendBuffer(&u8g2);
}

void draw_add_pin_screen(void)
{
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
	u8g2_DrawStr(&u8g2, 0, 12, "Add New PIN");
	u8g2_DrawStr(&u8g2, 0, 28, "Enter 4 digits:");
	u8g2_SendBuffer(&u8g2);
}

void draw_pin_added(void)
{
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
	u8g2_DrawStr(&u8g2, 0, 20, "PIN SAVED:");
	u8g2_DrawStr(&u8g2, 0, 40, pin_buffer);
	u8g2_SendBuffer(&u8g2);
}

void draw_remove_pin_menu(uint8_t cursor)
{
	char saved[5];
	char line[10];

	u8g2_ClearBuffer(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);

	u8g2_DrawStr(&u8g2, 0, 12, "Remove PIN:");

	uint8_t y = 28;

	for (uint8_t i = 0; i < MAX_PINS; i++)
	{
		eeprom_read_block(saved, stored_pins[i], 5);

		// Build "1: 1234"
		line[0] = '0' + (i + 1);
		line[1] = ':';
		line[2] = ' ';
		line[3] = saved[0];
		line[4] = saved[1];
		line[5] = saved[2];
		line[6] = saved[3];
		line[7] = '\0';

		if (i == cursor)
		{
			u8g2_DrawBox(&u8g2, 0, y - 10, 128, 12);
			u8g2_SetDrawColor(&u8g2, 0);
			u8g2_DrawStr(&u8g2, 2, y, line);
			u8g2_SetDrawColor(&u8g2, 1);
		}
		else
		{
			u8g2_DrawStr(&u8g2, 2, y, line);
		}

		y += 12;
	}

	u8g2_DrawStr(&u8g2, 0, 62, "* = Back   # = Delete");
	u8g2_SendBuffer(&u8g2);
}

void draw_lock_opened(void)
{
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);

	u8g2_DrawStr(&u8g2, 0, 32, "LOCK OPENED");
	u8g2_DrawStr(&u8g2, 0, 50, "Press * to lock");

	u8g2_SendBuffer(&u8g2);
}


// INPUT HANDLERS
void handle_main_menu_input(void)
{
	static char last = 0;
	char key = get_key_once();

	if (key && key != last)
	{
		if (key == '2')  // UP
		{
			if (selected > 0) selected--;
			if (selected < top) top = selected;
		}
		else if (key == '8') // DOWN
		{
			if (selected + 1 < MAIN_MENU_COUNT) selected++;
			if (selected > top + 3) top = selected - 3;
		}
		else if (key == '#') // SELECT
		{
			if (selected == 0) state = STATE_OPERATING_MODE;
			else if (selected == 1) state = STATE_PROGRAMMING_MENU;
		}
	}

	last = key;
}

void addPin() {
	clear_pin_buffer();

	while (pin_index < 4) {
		// Draw screen *every loop* so it updates as you type
		u8g2_ClearBuffer(&u8g2);
		u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);

		u8g2_DrawStr(&u8g2, 0, 12, "Add New PIN");
		u8g2_DrawStr(&u8g2, 0, 28, "Enter 4 digits:");
		u8g2_DrawStr(&u8g2, 0, 60, "* = Back");

		char display[6] = "____";
		for (uint8_t i = 0; i < pin_index; i++)
		display[i] = pin_buffer[i];

		u8g2_DrawStr(&u8g2, 0, 44, display);
		u8g2_SendBuffer(&u8g2);

		// Read keypad
		char k = get_key_once();

		// Allow cancel
		if (k == '*') {
			clear_pin_buffer();
			return;   // exit addPin() immediately
		}

		// Accept digits
		if (k >= '0' && k <= '9') {
			pin_buffer[pin_index++] = k;
		}
	}

	pin_buffer[4] = '\0';

	// Save immediately
	save_pin(pin_buffer);

	draw_pin_added();
	_delay_ms(1500);

	clear_pin_buffer();
}

void addFingerprint() {
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
	u8g2_DrawStr(&u8g2, 0, 32, "Place finger...");
	u8g2_SendBuffer(&u8g2);

	static uint16_t next_fid = 1;

	if (AS608_EnrollProcess(next_fid) == 1) {
		u8g2_ClearBuffer(&u8g2);
		u8g2_DrawStr(&u8g2, 0, 32, "Fingerprint Saved");
		u8g2_SendBuffer(&u8g2);
		next_fid++;
		} else {
		u8g2_ClearBuffer(&u8g2);
		u8g2_DrawStr(&u8g2, 0, 32, "Enroll Failed");
		u8g2_SendBuffer(&u8g2);
	}


	_delay_ms(1500);
}

void removeFingerprints(void)
{
	while (1)
	{
		u8g2_ClearBuffer(&u8g2);
		u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);

		u8g2_DrawStr(&u8g2, 0, 12, "Delete ALL Prints?");
		u8g2_DrawStr(&u8g2, 0, 32, "# = YES");
		u8g2_DrawStr(&u8g2, 0, 48, "* = NO");

		u8g2_SendBuffer(&u8g2);

		char key = get_key_once();
		if (!key) continue;

		if (key == '*')
		return;   // cancel

		if (key == '#')
		break;    // confirm
	}

	uint8_t result = AS608_EmptyDatabase();

	u8g2_ClearBuffer(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);

	if (result)
	u8g2_DrawStr(&u8g2, 0, 32, "Prints Deleted");
	else
	u8g2_DrawStr(&u8g2, 0, 32, "Delete Failed");

	u8g2_SendBuffer(&u8g2);
	_delay_ms(1500);
}



void handle_programming_input(void)
{
	char key = get_key_once();

	if (key == '*')
	state = STATE_MAIN_MENU;

	if (key == 'A') {
		addPin();
		state = STATE_PROGRAMMING_MENU;
	}
	if (key == 'B') {
		state = STATE_REMOVE_PIN_MENU;
	}
	if (key == 'C') {
		addFingerprint();
		state = STATE_PROGRAMMING_MENU;
	}
	if(key == 'D'){
		removeFingerprints();
		state = STATE_PROGRAMMING_MENU;
	}

}

void handle_operating_input(void)
{
	// Check fingerprint first
	if (AS608_ScanAndVerify()) {
		clear_pin_buffer();
		state = STATE_PIN_SUCCESS;
		return;
	}

	char key = get_key_once();

	if (key == '*') {
		clear_pin_buffer();
		state = STATE_MAIN_MENU;
		return;
	}

	if (key >= '0' && key <= '9') {
		if (pin_index < 4) {
			pin_buffer[pin_index++] = key;
		}
	}

	if (key == '#') {  // submit PIN
		if (pin_index == 4) {
			pin_buffer[4] = '\0';   
			if (check_pin()) {
				clear_pin_buffer();
				state = STATE_PIN_SUCCESS;
				} else {
				clear_pin_buffer();
				state = STATE_PIN_FAIL;
			}
		}
	}
}

uint8_t remove_cursor = 0;

void handle_remove_pin_menu_input(void)
{
	char key = get_key_once();
	if (!key) return;

	if (key == '*') {
		state = STATE_PROGRAMMING_MENU;
		return;
	}

	if (key == '2') { // UP
		if (remove_cursor > 0) remove_cursor--;
	}

	if (key == '8') { // DOWN
		if (remove_cursor < MAX_PINS - 1) remove_cursor++;
	}

	if (key == '#') { // DELETE
		delete_pin_slot(remove_cursor);
	}
}

void handle_unlocked_input(void)
{
	char key = get_key_once();

	if (key == '*')
	{
		PORTB &= ~(1 << PB4);
		state = STATE_MAIN_MENU;
	}
}

int main(void)
{
	DDRB |= (1<<PB3) | (1 << PB4);
	initLockServo();
		AS608_Init();
		_delay_ms(500);

	reset_all_pins();
	// SOFTWARE I2C
	u8g2_Setup_ssd1306_i2c_128x64_noname_f(
	&u8g2,
	U8G2_R0,
	u8x8_byte_sw_i2c,
	u8x8_gpio_and_delay
	);

	u8g2_SetI2CAddress(&u8g2, 0x3C << 1);
	u8g2_InitDisplay(&u8g2);
	u8g2_SetPowerSave(&u8g2, 0);

	while(1)
	{
		switch(state)
		{
			case STATE_MAIN_MENU:
			//enable LED Pins for lock
			PORTB |= (1 << PB3);
			lock();
			draw_main_menu();
			handle_main_menu_input();
			break;

			case STATE_PROGRAMMING_MENU:
			draw_programming_menu();
			handle_programming_input();
			break;

			case STATE_OPERATING_MODE:
			draw_operating_mode();
			handle_operating_input();
			break;

			case STATE_PIN_SUCCESS:
			draw_lock_opened();
			unlock();
			//enable LED Pins for lock/unlock
			PORTB |= (1 << PB4);
			PORTB &= ~(1 << PB3);
			handle_unlocked_input();
			break;

			case STATE_PIN_FAIL:
			draw_pin_fail();
			_delay_ms(1500);
			state = STATE_OPERATING_MODE;
			break;

			case STATE_REMOVE_PIN_MENU:
			draw_remove_pin_menu(remove_cursor);
			handle_remove_pin_menu_input();
			break;

		}

		_delay_ms(20);
	}
}
