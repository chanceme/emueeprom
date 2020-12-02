/*
* emueeprom.h
*/

#ifndef EMU_EEPROM_H
#define EMU_EEPROM_H

#include <unistd.h>

#include <flash.h>

#define BLOCK_START_ADDR 0x00000000

#define VADDR_SIZE 2u // bytes
#define SIZE_SIZE 2u // bytes
#define INFO_SIZE (VADDR_SIZE + SIZE_SIZE)
#define CRC_SIZE 2u // bytes
#define MIN_ENTRY_SIZE (INFO_SIZE + 1u)
#define MAX_DATA_PER_PAGE (PAGE_SIZE - INFO_SIZE - CRC_SIZE)

typedef struct {
    uint8_t pageBuffer[PAGE_SIZE];
    uint16_t bufferPos;
    uint16_t currPage;
    uint8_t currBlock;
} emueeprom_info_t;

void emuEepromInit(void);
void emuEepromDestroy(void);
void emuEepromInfo(emueeprom_info_t *pInfo);
ssize_t emuEepromWrite(uint16_t vAddr, void const *pBuffer, uint16_t buffLen);
ssize_t emuEepromRead(uint16_t vAddr, void *pBuffer, uint16_t buffLen);
ssize_t emuEepromErase(uint16_t vAddr,  uint16_t dataLen);
ssize_t emuEepromFlush(void);

#endif  // EMU_EEPROM_H