/*
* flash.h
*/

#include <unistd.h>

#include "flash_config.h"

int flashInit(void);
// void flashDestroy(void);
ssize_t flashWrite(off_t offset, void const *pBuff, size_t numBytes);
ssize_t flashRead(off_t offset, void *pBuff, size_t numBytes);
void flashBlockErase(int blockNum, int blockCount);