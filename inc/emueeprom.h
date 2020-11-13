/*
* emueeprom.h
*/

#ifndef EMU_EEPROM_H
#define EMU_EEPROM_H

#include <unistd.h>

// TESTING IDEA
// typedef struct {
//     ssize_t (*write)(off_t, void const *, size_t);
//     ssize_t (*read)(off_t, void *, size_t);
//     void (*erase)(int);
// } flash_fn_t;

void emuEepromInit(void);
void emuEepromDestroy(void);
ssize_t emuEepromWrite(uint16_t vAddr, void const *pBuffer, uint16_t buffLen);
ssize_t emuEepromRead(uint16_t vAddr, void *pBuffer, uint16_t buffLen);
ssize_t emuEepromErase(uint16_t vAddr,  uint16_t dataLen);
ssize_t emuEepromFlush(void);

#endif  // EMU_EEPROM_H