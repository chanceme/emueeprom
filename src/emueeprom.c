/*
* emueeprom.c
* 
* Notes:
* - Must used consectitive blocks.
*
*/

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <emueeprom.h>

#define HEADER_SIZE PAGE_SIZE
#define PAGES_PER_BLOCK (BLOCK_SIZE / PAGE_SIZE)
#define DATA_PAGES_PER_BLOCK (PAGES_PER_BLOCK - 1u)

#define BLOCK_DATA_OFFSET HEADER_SIZE

#define BITS_PER_BYTE 8u
#define ERASED 0xFF

// page buffer
#define VADDR_OFFSET 0u
#define SIZE_OFFSET 2u
#define DATA_OFFSET 4u
#define PAGE_CRC_OFFSET (PAGE_SIZE - CRC_SIZE)

#define MAX_VIRTUAL_ADDR (BLOCK_SIZE / 2) // < BLOCK_SIZE
#define VIRTUAL_ADDR_BITS (MAX_VIRTUAL_ADDR / BITS_PER_BYTE)
// Header
#define UNIQUE_ID 0xBEEF
#define INIT_CRC 0xFFFF

#define BUFFER_START 0x0000
#define PAGE_START 0x0001
#define TRANSFER_START 0x0000
#define TRANSFER_END 0xEEEE

typedef struct {
    uint16_t uniqueId; // user specific identifier
    uint16_t blockNum; // block number starting at 0
    uint16_t blockTotal; // total number of blocks used for emulated EEPROM
    uint16_t transferCount;
    uint16_t crc; // @TODO
} header_info_t;

typedef enum {
    block_start = 0,
    block_1 = 0,
    block_2,
    block_total,
    block_error
} blocks_t;

ssize_t _emuEepromBufferWrite(uint16_t vAddr, void const *pBuffer, uint16_t buffLen);
size_t _emuEepromPageRead(uint8_t *pPage, uint8_t *pBitmap, uint16_t vAddr, void *pBuffer, uint16_t buffLen);
ssize_t _emuEepromBlockRead(uint8_t *pBitmap, uint16_t vAddr, void *pBuffer, uint16_t buffLen);
ssize_t _emuEepromBlockTransfer(void);
void _emuEepromSetBit(uint16_t startAddr, uint16_t vAddr, uint8_t *pBitmap);
uint8_t _emuEepromReadBit(uint16_t startAddr, uint16_t vAddr, uint8_t *pBitmap);
ssize_t _emuEepromBlockFormat(blocks_t block, header_info_t header);
blocks_t _emuEepromActiveBlock(header_info_t *pHeader);
uint16_t _emuEepromCurrentPage(blocks_t block);
uint32_t _emuEepromFindAvailablePage(uint32_t offset, uint16_t change);
uint16_t _emuEepromHeaderCrc(header_info_t info);
uint16_t _emuEepromPageCrc(uint8_t *pBuffer);

static emueeprom_info_t m_info;
static bool m_init = false;

/*!------------------------------------------------------------------------------
    @brief Initializes emulated EEPROM.
    @param File descriptor from flash module.
    @return File descriptor or -1 if an error occured.
*///-----------------------------------------------------------------------------
void emuEepromInit(void)
{
    assert(!m_init);

    header_info_t header;

    m_info.currBlock = _emuEepromActiveBlock(&header);
    if(m_info.currBlock == block_error)
    {
        header.uniqueId = UNIQUE_ID;
        header.blockNum = block_1;
        header.blockTotal = block_total;
        header.transferCount = TRANSFER_START;
        header.crc = _emuEepromHeaderCrc(header);
        _emuEepromBlockFormat(block_1, header);
        m_info.currBlock = block_1;
        m_info.currPage = PAGE_START;
        m_info.bufferPos = BUFFER_START;
        printf("Emulated EEPROM created.\n");
    }
    else
    {
        printf("Emulated EEPROM found.\n");
        m_info.currPage = _emuEepromCurrentPage(m_info.currBlock);
        m_info.bufferPos = BUFFER_START;
        printf("Using block %d of %d.\nCurrent page: %d\n", m_info.currBlock + 1u, block_total, m_info.currPage);
    }

    memset(m_info.pageBuffer, ERASED, PAGE_SIZE);
    
    m_init = true;
}


/*!------------------------------------------------------------------------------
    @brief Erase blocks containing emulated EEPROM.
    @param None
    @return None
*///-----------------------------------------------------------------------------
void emuEepromDestroy(void)
{
    assert(m_init);

    flashBlockErase(block_start, block_total);
    m_init = false;
}


/*!------------------------------------------------------------------------------
    @brief Current info about emulated EEPROM.
    @param *pInfo - Pointer to the infomation.
    @return None
*///-----------------------------------------------------------------------------
void emuEepromInfo(emueeprom_info_t *pInfo)
{
    memcpy(pInfo->pageBuffer, m_info.pageBuffer, PAGE_SIZE);
    pInfo->bufferPos = m_info.bufferPos;
    pInfo->currPage = m_info.currPage;
    pInfo->currBlock = m_info.currBlock;
}


/*!------------------------------------------------------------------------------
    @brief Write data to emulated EEPROM.
    @param vAddr - Virtual address associated with the data being written.
    @param *pBuffer - Buffer of data to be written.
    @param buffLen - Amount of bytes being written.
    @return Amount of bytes written to emulated EEPROM or negative if error occured.
*///-----------------------------------------------------------------------------
ssize_t emuEepromWrite(uint16_t vAddr, void const *pBuffer, uint16_t buffLen)
{
    assert(m_init);
    assert(buffLen > 0);
    assert((vAddr + buffLen) <= MAX_VIRTUAL_ADDR);

    return _emuEepromBufferWrite(vAddr, pBuffer, buffLen);
}


/*!------------------------------------------------------------------------------
    @brief Read data back from the emulated EEPROM.
    @param vAddr - Virtual address of data to read.
    @param *pBuffer - Buffer to store read data.
    @param buffLen - Amount of bytes read.
    @return Amount of bytes read or negative number if error occured.
*///-----------------------------------------------------------------------------
ssize_t emuEepromRead(uint16_t vAddr, void *pBuffer, uint16_t buffLen)
{
    assert(m_init);
    assert(buffLen > 0);
    assert((vAddr + buffLen) <= MAX_VIRTUAL_ADDR);

    ssize_t count = 0;
    uint8_t *pBitmap =  NULL; 

    if(buffLen <= BITS_PER_BYTE)
    {
        pBitmap = calloc(1u, sizeof(uint8_t));
    }
    else
    {
        if(buffLen % BITS_PER_BYTE)
        {
            pBitmap = calloc((buffLen / BITS_PER_BYTE), sizeof(uint8_t));
        }
        else
        {
            pBitmap = calloc((buffLen / BITS_PER_BYTE) + 1u, sizeof(uint8_t));
        }
    }

    if(pBitmap != NULL)
    {

        if(m_info.bufferPos != BUFFER_START)
        {
            count = _emuEepromPageRead(m_info.pageBuffer, pBitmap, vAddr, pBuffer, buffLen);
            if((count >= 0) && (count != buffLen))
            {
                count = _emuEepromBlockRead(pBitmap, vAddr, pBuffer, buffLen);
            }
        }
        else
        {
            count = _emuEepromBlockRead(pBitmap, vAddr, pBuffer, buffLen);
        }

        free(pBitmap);
        pBitmap = NULL;
    }

    return count;
}


/*!------------------------------------------------------------------------------
    @brief Removes data related to virtual address from emulated EEPROM.
    @param vAddr - Virtual address of data to be erased.
    @param buffLen - Amount of data to erase.
    @return INFO_SIZE if successful or negative if failed.
*///-----------------------------------------------------------------------------
ssize_t emuEepromErase(uint16_t vAddr, uint16_t dataLen)
{
    assert(m_init);
    ssize_t count = 0;

    for(uint16_t i = vAddr; i < (vAddr + dataLen); i++)
    {
        count = _emuEepromBufferWrite(i, NULL, 0);
        if(count < 0)
        {
            break;
        }
    }

    return count;
}


/*!------------------------------------------------------------------------------
    @brief Write the current page buffer to flash.
    @param None
    @return Amount of bytes written to flash or negative number if error occured.
*///-----------------------------------------------------------------------------
ssize_t emuEepromFlush(void)
{
    assert(m_init);
    assert(m_info.currBlock < block_total);
    assert(m_info.currPage  < PAGES_PER_BLOCK);

    ssize_t count = 0;

    if(m_info.bufferPos != BUFFER_START)
    {
        // calculate the CRC for the page, store, then write to flash
        uint32_t currOffset = BLOCK_START_ADDR + (m_info.currBlock * BLOCK_SIZE) + (m_info.currPage * PAGE_SIZE);
        uint16_t calcCrc = _emuEepromPageCrc(m_info.pageBuffer);
        memcpy(&m_info.pageBuffer[PAGE_CRC_OFFSET], &calcCrc, sizeof(calcCrc));
        count = flashWrite(currOffset, m_info.pageBuffer, PAGE_SIZE);
        if(count > 0)
        {
            // reset info for page
            m_info.bufferPos = BUFFER_START;
            m_info.currPage++;
            memset(m_info.pageBuffer, ERASED, PAGE_SIZE);
            // if last page has been written to, initialize a block transfer
            if(m_info.currPage >= PAGES_PER_BLOCK)
            {
                count = _emuEepromBlockTransfer();
            }
        }
    }

    return count;
}


/*!------------------------------------------------------------------------------
    @brief Write data to buffer. 
    @param vAddr - Virtual address of data to be written.
    @param *pBUffer - Buffer containing the data to be written.
    @param buffLen - Amount of data (in bytes) to be written.
    @return Amount of data written or negative value if error occured.
*///-----------------------------------------------------------------------------
ssize_t _emuEepromBufferWrite(uint16_t vAddr, void const *pBuffer, uint16_t buffLen)
{
    ssize_t count = 0;
    uint16_t remainingSpace = ((PAGE_SIZE - CRC_SIZE) - m_info.bufferPos);

    if(remainingSpace >= (INFO_SIZE + buffLen)) 
    {
        memcpy(&m_info.pageBuffer[m_info.bufferPos + VADDR_OFFSET], &vAddr, sizeof(vAddr));
        memcpy(&m_info.pageBuffer[m_info.bufferPos + SIZE_OFFSET], &buffLen, sizeof(buffLen));
        if(buffLen)
        {
            memcpy(&m_info.pageBuffer[m_info.bufferPos + DATA_OFFSET], pBuffer, buffLen);
        }

        m_info.bufferPos += (INFO_SIZE + buffLen);
        count += buffLen;
    }
    else 
    {
        uint16_t writeCount = 0;
        uint16_t pagesNeeded = 0;     
        uint16_t remainder = 0;   

        remainingSpace -= INFO_SIZE;
        assert(buffLen > remainingSpace);
        if((buffLen - remainingSpace) % MAX_DATA_PER_PAGE)
        {
            remainder = 1u;
        }
        else
        {
            remainder = 0u;
        }

        pagesNeeded = ((buffLen - remainingSpace) / MAX_DATA_PER_PAGE) + remainder + 1u; // 1 for existing page  

        for(int i = 0; i < pagesNeeded; i++)
        {
            assert(remainingSpace != 0);
            memcpy(&m_info.pageBuffer[m_info.bufferPos + VADDR_OFFSET], &vAddr, sizeof(vAddr));
            memcpy(&m_info.pageBuffer[m_info.bufferPos + SIZE_OFFSET], &remainingSpace, sizeof(remainingSpace)); 
            memcpy(&m_info.pageBuffer[m_info.bufferPos + DATA_OFFSET], pBuffer + writeCount, remainingSpace); 
            m_info.bufferPos += (remainingSpace + INFO_SIZE);
            writeCount += remainingSpace;

            if(m_info.bufferPos >= PAGE_CRC_OFFSET)
            {
                count = emuEepromFlush();
            }

            if(count >= 0) 
            {   
                vAddr += remainingSpace;
                buffLen -= remainingSpace;
                if(buffLen == 0)
                {
                    break;
                }

                if(buffLen >= MAX_DATA_PER_PAGE)
                {
                    remainingSpace = MAX_DATA_PER_PAGE;
                }
                else
                {
                    remainingSpace = buffLen;
                }
            }
            else
            {
                break;
            }
        }
    }

    // check if min. entry can fit in current buffer
    if((m_info.bufferPos + INFO_SIZE) >= PAGE_CRC_OFFSET) 
    {
        ssize_t flushCount = emuEepromFlush();
        if(flushCount <= 0)
        {
            count = -1;
        }
    }

    return count;
}


/*!------------------------------------------------------------------------------
    @brief Read data from page. 
    @param *pPage - Buffer of data to be searched.
    @param *pBitmap - A bitmap corresponding to each virtual address.
    @param vAddr - Virtual address of data being searched for.
    @param *pBuffer - Buffer to store read data.
    @param buffLen - Amount of data (in bytes) to read.
    @return Amount of data (in bytes) read.
*///-----------------------------------------------------------------------------
size_t _emuEepromPageRead(uint8_t *pPage, uint8_t *pBitmap, uint16_t vAddr, void *pBuffer, uint16_t buffLen)
{
    size_t numRead = 0;
    uint16_t numEntries = 0; 
    uint8_t *pBuff = (uint8_t *)pBuffer;
    uint16_t *pEntries = NULL;

    // find all entries in buffer
    for(uint16_t i = 0; i < PAGE_CRC_OFFSET;)
    {
        uint16_t entryAddr = 0;
        uint16_t entrySize = 0;
        memcpy(&entryAddr, &pPage[i + VADDR_OFFSET], sizeof(entryAddr));
        memcpy(&entrySize, &pPage[i + SIZE_OFFSET], sizeof(entrySize));
        if((entryAddr != ERASED) && (entrySize < MAX_VIRTUAL_ADDR))
        {
            pEntries = (uint16_t *)realloc(pEntries, sizeof(*pEntries) + (sizeof(*pEntries) * numEntries));
            pEntries[numEntries] = i;
            i += VADDR_SIZE + SIZE_SIZE + entrySize;
            numEntries++;   
        }
        else
        {
            break;
        }
    }
    // search buffer starting with the last
    if(numEntries)
    {
        for(int i = (numEntries - 1); i >= 0; i--)
        {
            uint16_t tempVAddr = 0;
            uint16_t tempSize = 0;

            memcpy(&tempVAddr, &pPage[pEntries[i] + VADDR_OFFSET], sizeof(tempVAddr));
            memcpy(&tempSize, &pPage[pEntries[i] + SIZE_OFFSET], sizeof(tempSize));
            if(((tempVAddr <= vAddr) && (vAddr < (tempVAddr + tempSize))) || 
            ((tempVAddr < (vAddr + buffLen)) && ((vAddr + buffLen) <= (tempVAddr + tempSize)))||
            ((tempVAddr == vAddr) && (tempSize == 0)))
            {
                uint16_t amountFound = 0; // amount of bytes found in pPage
                uint16_t storeIndex = 0; // location in pBuffer to store read data
                uint16_t pageIndex = 0; // location to read from 
                uint16_t foundVAddr = 0;
        
                if(tempVAddr <= vAddr)
                {
                    storeIndex = 0;
                    pageIndex = (vAddr - tempVAddr);
                    foundVAddr = vAddr;
                    amountFound = (tempVAddr + tempSize) - vAddr;
                    if(amountFound >= buffLen)
                    {
                        amountFound = buffLen;
                    }

                }
                else
                {
                    storeIndex = (tempVAddr - vAddr);
                    pageIndex = 0;
                    foundVAddr = tempVAddr;
                    amountFound = (vAddr + buffLen) - tempVAddr;
                    if(amountFound >= tempSize)
                    {
                        amountFound = tempSize;
                    }
                }

                if(amountFound)
                {
                    for(int u = 0; u < amountFound; u++)
                    {   
                        if(_emuEepromReadBit(vAddr, foundVAddr, pBitmap) == 0)
                        {
                            memcpy(&pBuff[storeIndex + u], &pPage[pEntries[i] + DATA_OFFSET + pageIndex + u], sizeof(uint8_t));
                            _emuEepromSetBit(vAddr, foundVAddr, pBitmap);
                            numRead++;
                        }

                        foundVAddr++;
                    }
                }
                else // erased
                {
                    if(_emuEepromReadBit(vAddr, foundVAddr, pBitmap) == 0)
                    {
                        _emuEepromSetBit(vAddr, foundVAddr, pBitmap);
                        numRead++;
                        if(buffLen == 1u)
                        {
                            numRead = 0u;
                            break;
                        }
                    }
                }

                if(numRead == buffLen)
                {
                    break;
                }
            }
            else if((tempVAddr == vAddr) && (tempSize == 0)) // erased
            {
                break;
            }
        }
    }

    if(pEntries == NULL)
    {
        free(pEntries);
        pEntries = NULL;
    }

    return numRead;
}


/*!------------------------------------------------------------------------------
    @brief Read data from current block. 
    @param *pBitmap - A bitmap corresponding to each virtual address.
    @param vAddr - Virtual address of data being searched for.
    @param *pBuffer - Buffer to store read data to.
    @param buffLen - Amount of data (in bytes) to read.
    @return Amount of data (in bytes) read.
*///-----------------------------------------------------------------------------
ssize_t _emuEepromBlockRead(uint8_t *pBitmap, uint16_t vAddr, void *pBuffer, uint16_t buffLen)
{
    uint8_t pageBuffer[PAGE_SIZE] = {0};
    size_t numRead = 0;
    
    if(m_info.currPage > PAGE_START)
    {
        uint16_t lastPage = m_info.currPage - 1u;
        for(int i = lastPage; i >= 0; i--)
        {
            uint32_t currOffset = (BLOCK_START_ADDR + (m_info.currBlock * BLOCK_SIZE) + (i * PAGE_SIZE));
            ssize_t count = flashRead(currOffset, pageBuffer, PAGE_SIZE);
            if(count >= 0)
            {
                numRead += _emuEepromPageRead(pageBuffer, pBitmap, vAddr, pBuffer, buffLen);
                if(numRead == buffLen)
                {
                    break;
                }
            }
            else
            {
                return count;
            }
        }
    }

    return numRead;
}


/*!------------------------------------------------------------------------------
    @brief Transfer latest data to next block.
    @param None
    @return Last amount of data written to buffer or negative value if error occured.
*///-----------------------------------------------------------------------------
ssize_t _emuEepromBlockTransfer(void)
{
    uint8_t AddrBitMap[VIRTUAL_ADDR_BITS];
    uint8_t tempBuffer[PAGE_SIZE];
    uint32_t offset = (BLOCK_START_ADDR + (BLOCK_SIZE * m_info.currBlock));
    blocks_t lastBlock = m_info.currBlock;
    header_info_t header;

    memset(AddrBitMap, 0, VIRTUAL_ADDR_BITS);

    ssize_t count = flashRead(offset, &header, sizeof(header));
    if(count > 0)
    {
        m_info.currBlock++;
        if(m_info.currBlock == block_total)
        {
            m_info.currBlock = block_start;
        }

        if(header.transferCount >= TRANSFER_END)
        {
            header.transferCount = TRANSFER_START;
        }
        else
        {
            header.transferCount++;
        }

        header.blockNum = m_info.currBlock;
        header.crc = _emuEepromHeaderCrc(header);
        _emuEepromBlockFormat(m_info.currBlock, header);
        m_info.bufferPos = 0; 
        m_info.currPage = PAGE_START;

        for(uint16_t i = 0; i < DATA_PAGES_PER_BLOCK; i++)
        {
            offset = (((lastBlock * BLOCK_SIZE) + (BLOCK_SIZE - PAGE_SIZE)) - (PAGE_SIZE * i));
            count = flashRead(offset, tempBuffer, PAGE_SIZE);
            if(count > 0)
            {
                uint16_t tempCrc = _emuEepromPageCrc(tempBuffer);
                if(tempCrc == (uint16_t)(tempBuffer[PAGE_CRC_OFFSET] | tempBuffer[PAGE_CRC_OFFSET + 1] << BITS_PER_BYTE))
                {
                    uint16_t numEntries = 0, streak = 0, buffPos = 0;
                    uint16_t *pEntries = malloc(sizeof(uint16_t));

                    while(buffPos < (PAGE_CRC_OFFSET - MIN_ENTRY_SIZE))
                    {   
                        uint16_t entrySize = 0;
                        pEntries[numEntries++] = buffPos;
                        memcpy(&entrySize, &tempBuffer[buffPos + SIZE_OFFSET], sizeof(entrySize));
                        buffPos += (VADDR_SIZE + SIZE_SIZE + entrySize);
                        pEntries = realloc(pEntries, sizeof(*pEntries) + (numEntries * sizeof(uint16_t)));
                    }
    
                    for(int u = (numEntries - 1); u >= 0; u--)
                    {
                        uint16_t tempVAddr, tempSize, newVAddr;
                        memcpy(&tempVAddr, &tempBuffer[pEntries[u] + VADDR_OFFSET], sizeof(tempVAddr));
                        memcpy(&tempSize, &tempBuffer[pEntries[u] + SIZE_OFFSET], sizeof(tempSize));
                        newVAddr = tempVAddr;
                        // iterate through data, while checking bitmap
                        // if bitmap is already set, it will write the current streak of data
                        for(uint16_t z = 0; z <= tempSize; z++)
                        {
                            bool found = false;

                            if(z == tempSize)
                            {
                                found = true;
                            }
                            else if(_emuEepromReadBit(0u, newVAddr, AddrBitMap))
                            {
                                found = true;
                            }
                            
                            if(found)
                            {
                                if(streak)
                                {
                                    count = _emuEepromBufferWrite(tempVAddr, &tempBuffer[pEntries[u] + DATA_OFFSET + (z - streak)], streak);
                                    if(count > 0) 
                                    {
                                        for(uint16_t w = 0; w < streak; w++)
                                        {
                                            _emuEepromSetBit(0u, tempVAddr + w, AddrBitMap);
                                        }

                                        tempVAddr += ++streak;
                                        streak = 0;   
                                        found = false;                                  
                                    }
                                    else
                                    {
                                        break;
                                    }
                                }
                                else
                                {
                                    tempVAddr++;
                                }
                            }
                            else
                            {
                                streak++;
                            }

                            newVAddr++;
                        }

                        if(count <= 0)
                        {
                            break;
                        }
                    }
                                
                    free(pEntries);
                    pEntries = NULL;
                }
            }
        }

        flashBlockErase(lastBlock, 1u);
    }

    return count;
}


/*!------------------------------------------------------------------------------
    @brief Set bits in bitmap.
    @param vAddr - The virtual address to check.
    @param *pBitmap - The bitmap to read from.
    @param amount - Amount of bits to set.
    @return None
*///-----------------------------------------------------------------------------
void _emuEepromSetBit(uint16_t startAddr, uint16_t vAddr, uint8_t *pBitmap)
{
    assert(startAddr <= vAddr);
    uint16_t index = (vAddr - startAddr) / BITS_PER_BYTE;
    uint16_t bit = (vAddr - startAddr) % BITS_PER_BYTE;

    pBitmap[index] |= (1 << bit);
}


/*!------------------------------------------------------------------------------
    @brief Read bit in bitmap.
    @param vAddr - The virtual address to check.
    @param *pBitmap - The bitmap to read from.
    @return The value of the virtual address bit.
*///-----------------------------------------------------------------------------
uint8_t _emuEepromReadBit(uint16_t startAddr, uint16_t vAddr, uint8_t *pBitmap)
{
    assert(startAddr <= vAddr);
    uint8_t mask = 0x1;
    uint16_t index = (vAddr - startAddr) / BITS_PER_BYTE;
    uint16_t bit = (vAddr - startAddr) % BITS_PER_BYTE;

    return ((pBitmap[index] >> bit) & mask);

}


/*!------------------------------------------------------------------------------
    @brief Formats block by erasing entire block and writing header.
    @param block - The block to format.
    @param header - Header information about the block and emulated EEPROM.
    @return Amount of bytes written to flash or negative value if error occured.
*///-----------------------------------------------------------------------------
ssize_t _emuEepromBlockFormat(blocks_t block, header_info_t header)
{
    return flashWrite(BLOCK_START_ADDR + (BLOCK_SIZE * block), &header, sizeof(header));
}


/*!------------------------------------------------------------------------------
    @brief Find which block is the current block.
    @param *pHeader - Header information about the active block.
    @return Which block is currently active.
*///-----------------------------------------------------------------------------
blocks_t _emuEepromActiveBlock(header_info_t *pHeader)
{
    blocks_t foundBlock = block_error;
    uint16_t foundCount = 0u;

    for(blocks_t block = block_1; block < block_total; block++)
    {
        ssize_t count = flashRead((BLOCK_START_ADDR + (BLOCK_SIZE * block)), pHeader, sizeof(*pHeader));
        if(count >= 0)
        {
            if(pHeader->uniqueId == UNIQUE_ID)
            {
                if(foundBlock == block_error)
                {
                    foundBlock = block;
                    foundCount = pHeader->transferCount;
                }
                else
                {
                    if(foundCount == TRANSFER_END)
                    {
                        if(pHeader->transferCount == TRANSFER_START)
                        {
                            foundBlock = block;
                            foundCount = pHeader->transferCount;
                        }
                    }
                    else if((foundCount < pHeader->transferCount) && (pHeader->transferCount != TRANSFER_END))
                    {
                        foundBlock = block;
                        foundCount = pHeader->transferCount;
                    }
                }                            
            }
        }
        else
        {
            return block_error;
        }
    }
    
    return foundBlock;
}


/*!------------------------------------------------------------------------------
    @brief Find the current page.
    @param block - The block to search.
    @return The page number of the next avaiable page.
*///-----------------------------------------------------------------------------
uint16_t _emuEepromCurrentPage(blocks_t block)
{
    uint32_t offset = BLOCK_START_ADDR + (m_info.currBlock * BLOCK_SIZE) + (BLOCK_SIZE / 2u);

    return _emuEepromFindAvailablePage(offset, (BLOCK_SIZE / 2u)) % PAGE_SIZE;
}

/*!------------------------------------------------------------------------------
    @brief Recursively search for the next available page using a sort of binary search.
    @param offset - Offset in flash to read data from.
    @param change - The last amount of change in the offset.
    @return The page number of the next avaiable page.
*///-----------------------------------------------------------------------------
uint32_t _emuEepromFindAvailablePage(uint32_t offset, uint16_t change)
{
    uint16_t tempVAddr = 0;
    
    ssize_t count = flashRead(offset, &tempVAddr, sizeof(tempVAddr));
    if(count > 0)
    {
        if(tempVAddr <= MAX_VIRTUAL_ADDR) 
        {
            count = flashRead(offset + PAGE_SIZE, &tempVAddr, sizeof(tempVAddr));
            if(count > 0)
            {
                if((tempVAddr > MAX_VIRTUAL_ADDR) || (((offset + PAGE_SIZE) % BLOCK_SIZE) == 0))
                {
                    return ((offset + PAGE_SIZE) / PAGE_SIZE);
                }
                else
                {
                    offset += (change / 2u);
                }
            }
            else
            {
                return PAGE_START;
            }
        }
        else
        {
            count = flashRead(offset - PAGE_SIZE, &tempVAddr, sizeof(tempVAddr));
            if(count > 0)
            {
                if(tempVAddr <= MAX_VIRTUAL_ADDR)
                {
                    return ((offset / PAGE_SIZE) % PAGE_SIZE); 
                }
                else
                {
                    offset -= (change / 2u);
                }
            }
            else
            {
                return PAGE_START;
            }
        }
    }

    return _emuEepromFindAvailablePage(offset, (change / 2u));
}


/*!------------------------------------------------------------------------------
    @brief Calculate CRC for header.
    @param info - Header struct to calculate CRC from.
    @return CRC value.
*///-----------------------------------------------------------------------------
uint16_t _emuEepromHeaderCrc(header_info_t info)
{
    uint16_t crc = 0xCECE; 

    return crc;
}


/*!------------------------------------------------------------------------------
    @brief Calculate CRC for page.
    @param *pBuffer - Buffer to page data to calculate CRC from.
    @return CRC value.
*///-----------------------------------------------------------------------------
uint16_t _emuEepromPageCrc(uint8_t *pBuffer)
{
    uint16_t crc = 0xBEEF;

    return crc;
}