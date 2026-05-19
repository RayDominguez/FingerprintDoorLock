#ifndef LOCK_SERVO_H
#define LOCK_SERVO_H

#include <avr/io.h>

// Servo driver signal settings based on 15.625 kHz timer clock (Tceff = 64 us)
#define DEGREE0   13   // 800 us
#define DEGREE90  23   // 1500 us
#define DEGREE180 34   // 2200 us

// Initializes the I/O pin and Timer0 for the servo PWM signal
void initLockServo(void);

// Moves the servo to the locked position (180 degrees)
void lock(void);

// Moves the servo to the unlocked position (0 degrees)
void unlock(void);

#endif // LOCK_SERVO_H