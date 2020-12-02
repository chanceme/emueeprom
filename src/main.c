/*
* main.c
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <emueeprom.h>
#include <flash.h>
#include <test.h>

#define INPUT_MAX_SIZE 32u

int main()
{
    char str[INPUT_MAX_SIZE];
    int iVAddr = 0;
    int iValue = 0;
    ssize_t count = 0;

    int fd = flashInit();
    if(fd < 0)
    {
        printf("fd: %d\n", fd);
        return -1;
    }  

    emuEepromInit();
    printf("Limited functionality.\n");

    while(1)
    {
        printf("> ");
        fgets(str, INPUT_MAX_SIZE, stdin);

        if(!strcmp(str, "?\n") || !strcmp(str, "help\n"))
        {
            printf("'write'             - write decimal value to virtual address\n"
                    "'read'              - read decimal value stored at virtual address\n"
                    "'erase'             - erase data at virtual address\n"
                    "'flush'             - write current buffer to flash\n"
                    "'destroy'           - erases emulated eeprom from flash\n"
                    "'test'              - run emueeprom tests (warning: erases existing emulated eeprom)\n"
                    "'exit' or 'quit'    - exits program\n");
        }
        else if(!strcmp(str, "write\n"))
        {
            printf("Virtual address: ");
            fgets(str, INPUT_MAX_SIZE, stdin);
            iVAddr = atoi(str);
            printf("Value: ");
            fgets(str, INPUT_MAX_SIZE, stdin);
            iValue = atoi(str);
            count = emuEepromWrite(iVAddr, &iValue, sizeof(iValue));
            if(count <= 0)
            {
                printf("Error writting.\n");
            }
            else
            {
                printf("Wrote %d to %d.\n", iValue, iVAddr);
            }
            
        }
        else if(!strcmp(str, "read\n"))
        {
            printf("Virtual address: ");
            fgets(str, INPUT_MAX_SIZE, stdin);
            iVAddr = atoi(str);
            printf("Amount: ");
            fgets(str, INPUT_MAX_SIZE, stdin);
            iValue = atoi(str);
            count = emuEepromRead(iVAddr, &iValue, iValue);
            if(count < 0)
            {
                printf("Error reading.\n");
            }        
            else if(count == 0)
            {
                printf("Not found.\n");
            }    
            else if(count > 0)
            {
                printf("Value: %d\n", iValue);
            }
        }
        else if(!strcmp(str, "erase\n"))
        {
            printf("Virtual address: ");
            fgets(str, INPUT_MAX_SIZE, stdin);
            iVAddr = atoi(str);
            count = emuEepromErase(iVAddr, sizeof(iValue));
            if(count < 0)
            {
                printf("Error erasing.\n");
            }          
            else
            {
                printf("%d erased.", iVAddr);
            }
              
        }
        else if(!strcmp(str, "flush\n"))
        {
            count = emuEepromFlush();
            if(count > 0)
            {
                printf("Flushed.\n");
            }
            else
            {
                printf("Nothing to flush..\n");
            }
        }
        else if(!strcmp(str, "destroy\n"))
        {
            printf("Are you sure? [y/n]\n");
            fgets(str, INPUT_MAX_SIZE, stdin);
            if(!strcmp(str, "y\n") || !strcmp(str, "Y\n"))
            {
                emuEepromDestroy();
                printf("Shell commands will no longer work.\n");
            }
            else
            {
                printf("Input unknown.\n");
            }
            
        }
        else if(!strcmp(str, "test\n"))
        {
            int result = testSuiteEmuEeprom();
            if(result >= 0)
            {
                printf("Test Passed!\n");
            }
            else
            {
                printf("Test Failed.\n");
            }
            
        }
        else if((!strcmp(str, "exit\n")) || (!strcmp(str, "quit\n")))
        {
            break;
        }
    }

    return 0;
}