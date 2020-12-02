/*
* test.c
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <emueeprom.h>
#include <flash_config.h>
#include <test.h>

#define MIN_TEST_VIRT_ADDR 0u
#define MAX_TEST_VIRT_ADDR 128u
#define TEST_ERROR -1

int _testWriteRead(void);
int _testMultiPageWriteRead(void);
int _testBlockTransfer(void);
int _testEraseEntry(void);


/*!------------------------------------------------------------------------------
    @brief 
*///-----------------------------------------------------------------------------
int testSuiteEmuEeprom(void)
{
    // erase existing emueeprom
    emuEepromDestroy();
    emuEepromInit();

    printf("Starting test..\n");
    int result = _testWriteRead();
    if(result >= 0)
    {   
        printf("Single write/read passed.\n");
        result = _testMultiPageWriteRead();
        if(result >= 0)
        {
            printf("Multi-page write/read passed.\n");
            result = _testBlockTransfer();
            if(result >= 0)
            {
                printf("Transfer passed.\n");
                result = _testEraseEntry();
                if(result >= 0)
                {
                    printf("Erase passed.\n");
                }
            }
        }
    }

    return result;
}


/*!------------------------------------------------------------------------------
    @brief Write and read a single byte of data to the emulated EEPROM.
    @param None
    @return 0 if successful, -1 for error.
*///-----------------------------------------------------------------------------
int _testWriteRead(void)
{
    int result = TEST_ERROR;
    uint8_t testValue = 0x01;
    uint16_t vAddr = 1u;

    ssize_t amount = emuEepromWrite(vAddr, &testValue, sizeof(testValue));
    if(amount > 0)
    {
        uint8_t value = 0u;
        amount = emuEepromRead(vAddr, &value, sizeof(value));
        if(amount > 0u)
        {
            if(value == testValue)
            {
                result = 0;
            }
        }
    }

    return result;
}


/*!------------------------------------------------------------------------------
    @brief 
    @param None
    @return 0 if successful, -1 for error.
*///-----------------------------------------------------------------------------
int _testMultiPageWriteRead(void)
{
    int result = TEST_ERROR;
    uint8_t testArray[PAGE_SIZE] = {0};
    uint16_t vAddr = 50u;

    memset(testArray, 1u, PAGE_SIZE);

    ssize_t amount = emuEepromWrite(vAddr, &testArray, PAGE_SIZE);
    if(amount > 0)
    {
        uint8_t valueArray[PAGE_SIZE] = {0};
        amount = emuEepromRead(vAddr, &valueArray, PAGE_SIZE);
        if(amount > 0)
        {
            result = 0;
            for(int i = 0; i < PAGE_SIZE; i++)
            {
                if(valueArray[i] != testArray[i])
                {
                    result = TEST_ERROR; 
                    break;
                }
            }
        }
    }

    return result;
}


/*!------------------------------------------------------------------------------
    @brief
    @param None
    @return 0 if successful, -1 for error.
*///-----------------------------------------------------------------------------
int _testBlockTransfer(void)
{
    uint8_t testArray[PAGE_SIZE];
    uint16_t count = 0u;
    uint16_t vAddr = MIN_TEST_VIRT_ADDR;
    int result = 0;
    emueeprom_info_t info;
    uint8_t testBlock = 0;

    // get emueeprom infomation
    emuEepromInfo(&info);
    testBlock = info.currBlock;

    while(testBlock == info.currBlock)
    {
        for(uint16_t i = 0; i < PAGE_SIZE; i++)
        {
            testArray[i] = count++ % MAX_TEST_VIRT_ADDR;
        }

        ssize_t amount = emuEepromWrite(vAddr, testArray, PAGE_SIZE);
        if(amount < 0)
        {
            result = TEST_ERROR;
            break;
        }

        vAddr += PAGE_SIZE;
        if(vAddr >= MAX_TEST_VIRT_ADDR)
        {
            vAddr = MIN_TEST_VIRT_ADDR;
        }

        emuEepromInfo(&info);
    }

    for(uint16_t i = MIN_TEST_VIRT_ADDR; i < MAX_TEST_VIRT_ADDR; i++)
    {
        uint8_t data = 0;
        ssize_t amount = emuEepromRead(i, &data, sizeof(uint8_t));
        if((amount < 0) || (data != i))
        {
            result = TEST_ERROR;
            break;
        }        
    }

    return result;
}


/*!------------------------------------------------------------------------------
    @brief 
    @param None
    @return 0 if successful, -1 for error.
*///-----------------------------------------------------------------------------
int _testEraseEntry(void)
{
    uint16_t vAddr = 50u;
    int result = -1;

    ssize_t count = emuEepromErase(vAddr, sizeof(uint8_t));
    count = emuEepromFlush();
    if(count >= 0)
    {
        uint8_t data = 1u;
        count = emuEepromRead(vAddr, &data, sizeof(data));
        if(count == 0)
        {
            result = 0;
        }
    }

    return result;
}



