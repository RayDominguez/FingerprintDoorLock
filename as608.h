#ifndef AS608_H_
#define AS608_H_

#include <stdint.h>


// Initializes USART1 to 57600 baud for the AS608 sensor
void AS608_Init(void);


// Returns 1 if a finger is scanned and matches a template in the database, 0 otherwise
uint8_t AS608_ScanAndVerify(void);

// Master Enrollment Flow
// Pass the ID slot (0-300) where you want to save the new fingerprint.
// Returns 1 on success, 0 on failure. 
// NOTE: This function blocks and waits for user interaction!
uint8_t AS608_EnrollProcess(uint16_t new_id);


uint8_t AS608_GenImg(void);
uint8_t AS608_Img2Tz(void);
uint8_t AS608_Img2Tz2(void);
uint8_t AS608_Search(void);
uint8_t AS608_RegModel(void);
uint8_t AS608_Store(uint16_t id);
// Wipes the entire fingerprint database. Returns 1 on success, 0 on failure.
uint8_t AS608_EmptyDatabase(void);
// Deletes a specific fingerprint ID from the database
// Returns 1 on success, 0 on failure
uint8_t AS608_DeleteID(uint16_t id);

#endif /* AS608_H_ */