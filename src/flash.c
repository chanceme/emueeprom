/*
* flash.c
*/

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <flash.h>

// static bool m_Init = false;
static int m_fd = 0;

/*!------------------------------------------------------------------------------
    @brief Initializes flash by setting bin file to all 0xFF.
    @return File descriptor or -1 if an error occured.
*///-----------------------------------------------------------------------------
int flashInit(void)
{
    assert(!m_fd);

    if(access("flash.bin", F_OK) != -1)
    {
        m_fd = open("flash.bin", O_RDWR);
    }
    else
    {
        printf("Creating file..\n");
        m_fd = open("flash.bin", O_RDWR | O_CREAT);
        if(m_fd >= 0)
        {
            for(int i = 0; i < (FLASH_SIZE / BLOCK_SIZE); i++)
            {
                for(int u = 0; u < (BLOCK_SIZE / PAGE_SIZE); u++)
                {
                    uint8_t buffer[PAGE_SIZE];
                    memset(buffer, 0xFF, sizeof(buffer));

                    off_t loc = ((i * BLOCK_SIZE) + (u * PAGE_SIZE));
                    
                    off_t pos = lseek(m_fd, loc, SEEK_SET);
                    if(pos == loc)
                    {
                        int count = write(m_fd, buffer, sizeof(buffer));
                        if(count < 0)
                        {
                            printf("Error initializing file.\n");
                        }
                    }
                }
            }
        }
    }

    return m_fd;
}


/*!------------------------------------------------------------------------------
    @brief Write buffer to binary file.
    @param fd - File descriptor.
    @param offset - Offset from start of file to write to.
    @param *pBuffer - Buffer with the data to be written.
    @param numBytes - Number of byte to be written.
    @return Number of bytes written or -1 if an error occured.
*///-----------------------------------------------------------------------------
ssize_t flashWrite(off_t offset, void const *pBuff, size_t numBytes)
{
    assert(m_fd);
    assert(numBytes);

    ssize_t count;

    off_t pos = lseek(m_fd, offset, SEEK_SET);
    if(pos == offset)
    {
        count = write(m_fd, pBuff, numBytes);
        if(count < 0)
        {
            printf("Error writing to file.\n");
        }
    }

    return count;
}


/*!------------------------------------------------------------------------------
    @brief Read binary file to buffer.
    @param fd - File descriptor.
    @param offset - Offset from start of file to read from.
    @param *pBuffer - Buffer to store read data.
    @param numBytes - Number of byte to be read.
    @return Number of bytes written or -1 if an error occured.
*///-----------------------------------------------------------------------------
ssize_t flashRead(off_t offset, void *pBuff, size_t numBytes)
{
    assert(m_fd);
    assert(numBytes);

    ssize_t count;

    off_t pos = lseek(m_fd, offset, SEEK_SET);
    if(pos == offset)
    {
        count = read(m_fd, pBuff, numBytes);
        if(count < 0)
        {
            printf("Error reading from file.\n");
        }
    }

    return count;
}


/*!------------------------------------------------------------------------------
    @brief Erase a block by writting all 0xFF.
    @param fd - File descriptor.
    @param blockNum - Which block to erase.
    @return None.
*///-----------------------------------------------------------------------------
void flashBlockErase(int blockNum, int blockCount)
{
    assert(m_fd);
    assert((blockNum * BLOCK_SIZE) <= (FLASH_SIZE - BLOCK_SIZE));

    for(int u = 0; u < (BLOCK_SIZE / PAGE_SIZE); u++)
    {
        uint8_t buffer[PAGE_SIZE];
        memset(buffer, 0xFF, sizeof(buffer));

        off_t loc = ((blockNum * BLOCK_SIZE) + (u * PAGE_SIZE));
        
        off_t pos = lseek(m_fd, loc, SEEK_SET);
        if(pos == loc)
        {
            int count = write(m_fd, buffer, sizeof(buffer));
            if(count < 0)
            {
                printf("Error erasing block.\n");
            }
        }
    }
}
