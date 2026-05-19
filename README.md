# FingerprintDoorLock
We will be making a fingerprint-controlled lock with an OLED display and an additional keypad for entry as well. 

We also plan to include some LEDs as additional feedback for success/failure. In the first state, the lock is closed and the LCD prompts the user to select programming or operating mode. The user uses a 4x4 keypad to select either mode. 
In programming mode, the user can add or remove a fingerprint or a 4-digit pin number. 

In operating mode, the user can enter a pin number or scan their fingerprint to open the lock. If there is a record match with stored prints or the correct pin number is entered then it will open the lock and show access was granted on the OLED display as well as making the blue LED turn on. If there is no record match then the lock will stay closed and say access was denied with the red LED on. 

To open and close the lock we will use a servo motor as the lock mechanism. The 5 peripherals we will use are the servo motor, fingerprint sensor, OLED display, LEDs, and a keypad. Our 2 new peripherals will be the OLED display and the fingerprint sensor. The two input peripherals will be the fingerprint sensor and a keypad.

Our main objective was to create a biometric security system that allowed a user to access a specific system with only their fingerprint.  Our project successfully demonstrated the system being able to manage multiple inputs and outputs to control the physical access to a door, box, etc. We successfully integrated our ATmega328PB with an AS608 Fingerprint Sensor, keypad and a servo motor to act as the physical lock.  Rather than relying on hardcoded pins and fingerprints, we added a programming feature that allowed users to add their own fingerprint or pin to unlock the system.
