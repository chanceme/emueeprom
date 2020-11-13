/*
* main.c
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <emueeprom.h>
#include <flash.h>

int main()
{
    char str[100];
    int value = 0;
    int vAddr = 0;
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
        scanf("%s", str);

        if(!strcmp(str, "?") || !strcmp(str, "help"))
        {
            printf("'write'   - write value to virtual address\n"
                    "'read'    - read value stored at virtual address\n"
                    "'erase'   - erase data at virtual address\n"
                    "'flush'   - write current buffer to flash\n"
                    "'destroy' - erases emulated eeprom from flash\n");
        }
        else if(!strcmp(str, "write"))
        {
            printf("Virtual address: ");
            scanf("%d", &vAddr);
            printf("Value: ");
            scanf("%d", &value);
            count = emuEepromWrite(vAddr, &value, sizeof(value));
            if(count <= 0)
            {
                printf("Error writting.\n");
            }
            else
            {
                printf("Wrote %d to %d.\n", value, vAddr);
            }
            
        }
        else if(!strcmp(str, "read"))
        {
            printf("Virtual address: ");
            scanf("%d", &vAddr);
            count = emuEepromRead(vAddr, &value, sizeof(value));
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
                printf("Value: %d\n", value);
            }
        }
        else if(!strcmp(str, "erase"))
        {
            printf("Virtual address: ");
            scanf("%d", &vAddr);
            count = emuEepromErase(vAddr, sizeof(value));
            if(count < 0)
            {
                printf("Error erasing.\n");
            }          
            else
            {
                printf("%d erased.", vAddr);
            }
              
        }
        else if(!strcmp(str, "flush"))
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
        else if(!strcmp(str, "destroy"))
        {
            printf("Are you sure? [y/n]\n");
            scanf("%s", str);
            if(!strcmp(str, "y") || !strcmp(str, "Y"))
            {
                emuEepromDestroy();
                printf("Shell commands will no longer work.\n");
            }
        }
    }

    return 0;
}