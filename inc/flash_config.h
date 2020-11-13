/*
* flash_config.h
*/

#ifndef FLASH_CONFIG_H
#define FLASH_CONFIG_H

#ifdef LINUX
    #define FLASH_SIZE 65536u // bytes
    #define BLOCK_SIZE 4096u // bytes
    #define PAGE_SIZE 32u // bytes
#endif

#ifdef NXP_KW41Z
    #error Not yet supported.
#endif



#endif // FLASH_CONFIG_H