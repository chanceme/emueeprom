/*
* emueeprom.c
* 
* Notes:
* - Must used consectitive blocks.
*
* Todos:
* - Add support for different amount of blocks used
*/

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <emueeprom.h>
#include <flash.h>

#define HEADER_SIZE PAGE_SIZE
#define PAGES_PER_BLOCK (BLOCK_SIZE / PAGE_SIZE)

#define VADDR_SIZE 2u // bytes
#define SIZE_SIZE 2u // bytes
#define ENTRY_SIZE (VADDR_SIZE + SIZE_SIZE)
#define CRC_SIZE 2u // bytes
#define MAX_DATA_PER_PAGE (PAGE_SIZE - ENTRY_SIZE - CRC_SIZE)

#define BLOCK_START_ADDR 0x00000000
#define BLOCK_DATA_OFFSET HEADER_SIZE

// page buffer
#define VADDR_OFFSET 0u
#define SIZE_OFFSET 2u
#define DATA_OFFSET 4u
#define PAGE_CRC_OFFSET (PAGE_SIZE - CRC_SIZE)

#define MAX_VIRTUAL_ADDR (BLOCK_SIZE / 2) // < BLOCK_SIZE
#define VIRTUAL_ADDR_BITS (MAX_VIRTUAL_ADDR / sizeof(uint8_t))
// Header
#define UNIQUE_ID 0xBEEF
#define INIT_CRC 0xFFFF

// Buffer Position
#define BUFFER_START 0x0000

// Page Count
#define PAGE_START 0x0001

// Transfer count
#define TRANSFER_START 0x0000
#define TRANSFER_END 0xEEEE

#define BITS_PER_BYTE 8u

typedef struct {
    uint8_t pageBuffer[PAGE_SIZE];
    uint16_t bufferPos;
    uint16_t currPage;
    uint8_t currBlock;
} emueeprom_info_t;

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
void _emuEepromSetBit(uint16_t vAddr, uint8_t *pBitmap, uint16_t buffLen);
uint8_t _emuEepromReadBit(uint16_t vAddr, uint8_t *pBitmap);
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
        m_info.currPage = PAGE_START;
        m_info.bufferPos = BUFFER_START;
        printf("Emulated EEPROM created.\n");
    }
    else
    {
        printf("Emulated EEPROM found.\n");
        m_info.currPage = _emuEepromCurrentPage(m_info.currBlock);
        m_info.bufferPos = BUFFER_START;
        printf("Using block %d of %d.\nCurrent page: %d\n", m_info.currBlock + 1, block_total, m_info.currPage);
    }

    memset(m_info.pageBuffer, 0u, PAGE_SIZE);
    
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
    uint8_t *pBitmap; 

    if(buffLen > BITS_PER_BYTE)
    {
        pBitmap = malloc((buffLen / BITS_PER_BYTE) + sizeof(uint8_t));
        memset(pBitmap, 0, (buffLen / BITS_PER_BYTE) + sizeof(uint8_t));
    }
    else
    {
        pBitmap = malloc(sizeof(uint8_t));
        memset(pBitmap, 0, sizeof(uint8_t));
    }
        
    count = _emuEepromPageRead(m_info.pageBuffer, pBitmap, vAddr, pBuffer, buffLen);
    if((count >= 0) && (count != buffLen))
    {
        count = _emuEepromBlockRead(pBitmap, vAddr, pBuffer, buffLen);
    }

    free(pBitmap);

    return count;
}


/*!------------------------------------------------------------------------------
    @brief Removes data related to virtual address from emulated EEPROM.
    @param vAddr - Virtual address of data to be erased.
    @param buffLen - Amount of data to erase.
    @return ENTRY_SIZE if successful or negative if failed.
*///-----------------------------------------------------------------------------
ssize_t emuEepromErase(uint16_t vAddr, uint16_t dataLen)
{
    assert(m_init);
    ssize_t count = 0;

    for(;vAddr < (vAddr + dataLen); vAddr++)
    {
        count = _emuEepromBufferWrite(vAddr, NULL, 0);
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
    ssize_t count = 0;

    // if last page has been written to, initialize a block transfer
    if(m_info.currPage == PAGES_PER_BLOCK)
    {
        count = _emuEepromBlockTransfer();
    }

    if(count >= 0)
    {
        if(m_info.bufferPos != BUFFER_START)
        {
            // calculate the CRC for the page, store, then write to flash
            uint32_t currOffset = BLOCK_START_ADDR + (m_info.currBlock * BLOCK_SIZE) + (m_info.currPage * PAGE_SIZE);
            uint16_t calcCrc = _emuEepromPageCrc(m_info.pageBuffer);
            memcpy(&m_info.pageBuffer[PAGE_CRC_OFFSET], &calcCrc, sizeof(calcCrc));
            count = flashWrite(currOffset, m_info.pageBuffer, PAGE_SIZE);

            // reset info for page
            m_info.bufferPos = PAGE_START;
            m_info.currPage++;
            memset(m_info.pageBuffer, 0, PAGE_SIZE);
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
    // check if min. entry can fit in current buffer
    if((m_info.bufferPos + ENTRY_SIZE) >= PAGE_CRC_OFFSET) 
    {
        count = emuEepromFlush();
    }
    
    if(count >= 0)
    {
        uint16_t remainingSpace = ((PAGE_SIZE - CRC_SIZE) - m_info.bufferPos);
        count++;

        if(remainingSpace >= (ENTRY_SIZE + buffLen)) // write normally
        {
            memcpy(&m_info.pageBuffer[m_info.bufferPos + VADDR_OFFSET], &vAddr, sizeof(vAddr));
            memcpy(&m_info.pageBuffer[m_info.bufferPos + SIZE_OFFSET], &buffLen, sizeof(buffLen));
            memcpy(&m_info.pageBuffer[m_info.bufferPos + DATA_OFFSET], pBuffer, buffLen);

            m_info.bufferPos += (ENTRY_SIZE + buffLen);

            count += buffLen;
        }
        else // multiple pages needed
        { 
            uint8_t remainder = ((buffLen - remainingSpace) % MAX_DATA_PER_PAGE);
            uint16_t pagesNeeded = ((buffLen - remainingSpace) / MAX_DATA_PER_PAGE) + remainder + 1u; // 1 for existing page

            for(int i = 0; i < pagesNeeded; i++)
            {
                memcpy(&m_info.pageBuffer[m_info.bufferPos + VADDR_OFFSET], &vAddr, sizeof(vAddr));
                memcpy(&m_info.pageBuffer[m_info.bufferPos + SIZE_OFFSET], &remainingSpace, sizeof(remainingSpace));
                memcpy(&m_info.pageBuffer[m_info.bufferPos + DATA_OFFSET], pBuffer, remainingSpace);
                m_info.bufferPos += remainingSpace;

                if(m_info.bufferPos == PAGE_CRC_OFFSET)
                {
                    count = emuEepromFlush();
                }

                if(count >= 0)
                {
                    buffLen -= remainingSpace;
                    vAddr += remainingSpace;

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
    uint16_t *pEntries;

    pEntries = malloc(sizeof(*pEntries));

    // find all entries in buffer
    for(uint16_t i = 0; i < PAGE_SIZE; i++)
    {
        uint16_t entrySize = 0;
        memcpy(&entrySize, &pPage[i + SIZE_OFFSET], sizeof(entrySize));
        if((entrySize > 0) && (entrySize < MAX_VIRTUAL_ADDR))
        {
            pEntries[numEntries] = i;
            i += VADDR_SIZE + SIZE_SIZE + entrySize;
            numEntries++;
            pEntries = realloc(pEntries, sizeof(*pEntries) + (sizeof(*pEntries) * numEntries));
        }
        else
        {
            break;
        }
    }
    // search buffer starting with the last/end
    if(numEntries)
    {
        for(int i = numEntries; i > 0; i--)
        {
            uint16_t tempVAddr = 0;
            uint16_t tempSize = 0;
            uint16_t amountFound = 0; // amount of bytes found in pPage
            uint16_t storeIndex = 0; // location in pBuffer to store read data
            uint16_t foundIndex = 0; // location in pPage to start reading from

            memcpy(&tempVAddr, &pPage[pEntries[i] + VADDR_OFFSET], sizeof(tempVAddr));
            memcpy(&tempSize, &pPage[pEntries[i] + SIZE_OFFSET], sizeof(tempSize));
            
            if(((tempVAddr <= vAddr) && (vAddr < (tempVAddr + tempSize))) || 
            ((tempVAddr < (vAddr + buffLen)) && ((vAddr + buffLen) <= (tempVAddr + tempSize))))
            {
                if(tempVAddr <= vAddr)
                {
                    foundIndex = (vAddr - tempVAddr);
                    storeIndex = 0;
                    amountFound = (tempVAddr + tempSize) - vAddr;
                    if(amountFound >= buffLen)
                    {
                        amountFound = buffLen;
                    }
                }
                else
                {
                    foundIndex = 0;
                    storeIndex = (tempVAddr - vAddr);
                    amountFound = (vAddr + buffLen) - tempVAddr;
                    if(amountFound >= tempSize)
                    {
                        amountFound = tempSize;
                    }
                }
                
                for(int u = 0; u < amountFound; u++)
                {   
                    
                    uint16_t foundVAddr = foundIndex + tempVAddr + u;
                    // could be fragmented and require multiple entries
                    if(_emuEepromReadBit(foundVAddr, pBitmap) == 0)
                    {
                        memcpy(&pBuff[storeIndex + u], &pPage[foundIndex + u + DATA_OFFSET] , sizeof(uint8_t));
                        _emuEepromSetBit(foundVAddr, pBitmap, sizeof(uint8_t));
                        numRead++;
                    }
                }

                if(numRead == buffLen)
                {
                    break;
                }
            }
        }
    }
    
    free(pEntries);

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
    
    if(m_info.currPage != 0)
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
    @note TODO - Clean up using _emuEepromPageRead()
*///-----------------------------------------------------------------------------
ssize_t _emuEepromBlockTransfer(void)
{
    uint8_t AddrBitMap[VIRTUAL_ADDR_BITS];
    uint8_t tempBuffer[PAGE_SIZE];
    uint32_t offset = (BLOCK_START_ADDR + (BLOCK_SIZE * m_info.currBlock));
    blocks_t lastBlock = m_info.currBlock;
    header_info_t header;

    ssize_t count = flashRead(offset, &header, sizeof(header));
    if(count > 0)
    {
        if((m_info.currBlock + 1) == block_total)
        {
            m_info.currBlock = block_start;
        }
        else
        {
            m_info.currBlock = m_info.currBlock + 1;
        }

        if(header.transferCount == TRANSFER_END)
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

        for(uint16_t i = 0; i < PAGES_PER_BLOCK; i++)
        {
            // checking bitmap, write virtual address data to new block, updating bitmap
            offset = (((m_info.currBlock * BLOCK_SIZE) + (BLOCK_SIZE - PAGE_SIZE)) - (PAGE_SIZE * i));
            count = flashRead(offset, tempBuffer, PAGE_SIZE);
            if(count > 0)
            {
                // validate
                uint16_t tempCrc = _emuEepromPageCrc(tempBuffer);
                if(tempCrc == (uint16_t)tempBuffer[PAGE_CRC_OFFSET])
                {
                    uint16_t numEntries, streak = 0, buffPos = 0;
                    uint16_t *pEntries = malloc(sizeof(uint16_t));

                    while(buffPos < PAGE_CRC_OFFSET)
                    {   
                        uint16_t entrySize = 0;
                        pEntries[numEntries++] = buffPos;
                        memcpy(&entrySize, &tempBuffer[buffPos + SIZE_OFFSET], sizeof(entrySize));
                        buffPos += (VADDR_SIZE + SIZE_SIZE + entrySize);
                        pEntries = realloc(pEntries, sizeof(*pEntries) + (numEntries * sizeof(uint16_t)));
                    }

                    for(uint16_t u = (numEntries - 1); u >= 0; u--)
                    {
                        uint16_t tempVAddr, tempSize;
                        memcpy(&tempVAddr, &tempBuffer[pEntries[u] + VADDR_OFFSET], sizeof(tempVAddr));
                        memcpy(&tempSize, &tempBuffer[pEntries[u] + SIZE_OFFSET], sizeof(tempSize));
                        // iterate through data, while checking bitmap
                        // if bitmap is already set, it will write the current streak of data
                        for(uint16_t z = 0; z <= tempSize; z++)
                        {
                            if(_emuEepromReadBit(tempVAddr + z, AddrBitMap) || (z == tempSize))
                            {
                                if(streak)
                                {
                                    count = _emuEepromBufferWrite(tempVAddr, &tempBuffer[pEntries[u] + DATA_OFFSET + (z - streak)], streak);
                                    if(count > 0) 
                                    {
                                        _emuEepromSetBit(tempVAddr, AddrBitMap, streak);
                                        tempVAddr += streak;
                                        streak = 0;
                                    }
                                    else
                                    {
                                        break;
                                    }
                                }

                                tempVAddr++;
                            }
                            else
                            {
                                streak++;
                            }
                        }

                        if(count <= 0)
                        {
                            break;
                        }
                    }
                                
                    free(pEntries);
                }
            }

            flashBlockErase(lastBlock, 1u);
        }
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
void _emuEepromSetBit(uint16_t vAddr, uint8_t *pBitmap, uint16_t amount)
{
    for(uint16_t i = vAddr; i < (vAddr + amount); i++)
    {
        pBitmap[vAddr / 8u] = 1 << (vAddr % 8u);
    }
}


/*!------------------------------------------------------------------------------
    @brief Read bit in bitmap.
    @param vAddr - The virtual address to check.
    @param *pBitmap - The bitmap to read from.
    @return The value of the virtual address bit.
*///-----------------------------------------------------------------------------
uint8_t _emuEepromReadBit(uint16_t vAddr, uint8_t *pBitmap)
{
    uint8_t mask = 0x1;
    uint8_t index = vAddr % 8u;
    uint8_t value = pBitmap[vAddr / 8u];

    return ((value >> index) & mask);
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
    uint16_t foundCount = 0;

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

    return _emuEepromFindAvailablePage(offset, (BLOCK_SIZE / 2u));
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
        if(tempVAddr <= MAX_VIRTUAL_ADDR) // found entry
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
                return 0u;
            }
        }
        else // not found, look further back
        {
            count = flashRead(offset - PAGE_SIZE, &tempVAddr, sizeof(tempVAddr));
            if(count > 0)
            {
                if(tempVAddr <= MAX_VIRTUAL_ADDR)
                {
                    return (offset / PAGE_SIZE); 
                }
                else
                {
                    offset -= (change / 2u);
                }
            }
            else
            {
                return 0u;
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
    uint16_t crc = 0xCECE; // temp until I figure out what's wrong with CRC..
    // uint16_t crc = INIT_CRC;

    // crc = crc16(&info.uniqueId, sizeof(info.uniqueId), crc);
    // crc = crc16(&info.blockNum, sizeof(info.blockNum), crc);
    // crc = crc16(&info.blockTotal, sizeof(info.blockTotal), crc);

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