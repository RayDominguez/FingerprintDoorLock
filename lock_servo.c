#include "lock_servo.h"

void initLockServo(void) {
	//init IO for PB2
    DDRB |= (1 << PINB2);     // Set PB2 as an output


    // Clear OC1B on Compare Match, set OC1B at BOTTOM (Non-inverting mode)
    //Fast PWM, 8-bit 
    TCCR1A = (1 << COM1B1) | (1 << WGM10);
    
    // Clock prescaler = 1024 
    // Timer frequency = 15.6kHz; PWM frequency = 61 Hz
    TCCR1B = (1 << WGM12) | (1 << CS12) | (1 << CS10);
    
    TCNT1 = 0;                // Initialize 16-bit counter
}

void lock(void) {
    // Set servo motor to 180 degrees using Timer1 Channel B
    OCR1B = DEGREE180;
}

void unlock(void) {
    // Set servo motor to 0 degrees using Timer1 Channel B
    OCR1B = DEGREE0;
}