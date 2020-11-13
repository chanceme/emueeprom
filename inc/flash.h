/*
* flash.h
*/

#ifndef FLASH_H
#define FLASH_H

#include <unistd.h>

#include "flash_config.h"

int flashInit(void);
ssize_t flashWrite(off_t offset, void const *pBuff, size_t numBytes);
ssize_t flashRead(off_t offset, void *pBuff, size_t numBytes);
void flashBlockErase(int blockNum, int blockCount);

#endif // FLASH_H