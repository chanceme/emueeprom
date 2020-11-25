/*
* test.c
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <emueeprom.h>
#include <flash_config.h>
#include <test.h>

int _testWriteRead(void);
int _testMultiPageWriteRead();
// _testBlockTransfer(void);
// _ testEraseEntry(void);


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
    // int result = 0;
    if(result >= 0)
    {   
        printf("Single write/read passed.\n");
        result = _testMultiPageWriteRead();
        if(result >= 0)
        {
            printf("Multi-page write/read passed.\n");
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
    int result = -1;
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
                result = 0u;
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
    int result = -1;
    uint8_t testArray[PAGE_SIZE] = {0};
    uint16_t vAddr = 100u;

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
                    for(int u = 0; u < (PAGE_SIZE - i); u++)
                    {
                        printf("valueArray[%d] = %d\ntestArray[%d] = %d\n", i + u, valueArray[i + u], i + u, testArray[i + u]);
                    }
                    result = -1; 
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
// int testBlockTransfer(void)
// {

// }


/*!------------------------------------------------------------------------------
    @brief 
    @param None
    @return 0 if successful, -1 for error.
*///-----------------------------------------------------------------------------
// int testEraseEntry(void)
// {

// }



