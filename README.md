![emueeprom](img/emueeprom_logo.png?raw=true)

# emueeprom (Emulated EEPROM)

## Getting Started

You can clone the repository using git by issuing the following command:

```
$ git clone https://github.com/chanceme/emueeprom.git
```

To build this project and test it's basic features in a terminal, issue the following command in the src folder:

```
$ make
gcc -I../inc  -Wall -DLINUX    -c -o main.o main.c
gcc -I../inc  -Wall -DLINUX    -c -o flash.o flash.c
gcc -I../inc  -Wall -DLINUX    -c -o emueeprom.o emueeprom.c
gcc -o emueeprom main.o flash.o emueeprom.o -I../inc  -Wall -DLINUX 
```

To run the program:

```
$ ./emueeprom
```

Enter 'help' or '?' for commands.

## Goals/To-Dos

The main goal is to create a simple to use, wear-leveling application that can be used by embedded devices. Other goals and to-dos are:

* Get repository created, initial push
* Clean existing code
* Add working CRC16
* Add to Overview Section
* Create and pass tests suite on a Linux system
* Add doxygen documents
* Implement input from others for improvements and fixes
* Add support for other platforms
* Add support for external devices

## Supported Devices

* Linux (11/12/2020)
* NXP KW41Z (Coming Soon)
* Nordic Semiconductor nRF52840 (Coming Soon)

### Add New Support

* Add device specific flash.c that share same header
* Add device specific flash parameters to flash_config.h
* Change preprocessor value in Makefile

## Overview

The perk of EEPROM (NOR) over a regular flash (NAND) is that it can read, write/program, and erase single bytes of data at a time where regular flash requires the user to read or write/program in pages (multiple bytes) and erase in blocks (multiple pages). Emulating an EEPROM on regular flash allows the user to have smaller minimum write size, more flexibility, and faster write speeds.  

In this document/example, the flash size is 64KB, page size of 32 bytes, and a block size of 4KB (also used in the example code).  

## Design

The emulated EEPROM works by using at least 2 blocks (minimum erase size of the flash) and filling one block at a time with data. Once that block becomes full, data is then transferred from the current block to the new block. Only the latest information will be moved and the duplicates will be erase. To specify data, a virtual address is used. The virtual address is a value that the user can define.

### Initialization

When the emulated EEPROM is initialized, a single block (user defined address) writes a header the contains information about the emulated EEPROM. The header contains the following;

* Unique ID - User specific value.
* Block Number - Which block this currently is out of the total amount.
* Block Total - The total amount of blocks used.
* Block Count - Increments every time a transfer is done.
* CRC - A CRC value of the three values above.

Only a single block will have a formated header a single time. This allows the program to determine which block is the active block during start up by checking for the unique ID and validating the CRC. An overview of the two blocks used in this example is shown in Figure 1.

```
                        BLOCK 1                                 BLOCK 2
                     _______________                         _______________
0x00007FFF  ----->  |               |   0x0000FFFF  ----->  |               |
                    |               |                       |               |
                    |               |                       |               |
                    |               |                       |               |
                    |_______________|                       |_______________|

                            :                                       :
                     _______________                         _______________    
                    |               |                       |               |
                    |               |                       |               |
                    |               |                       |               |
                    |_______________|                       |_______________|
0x00000000  ----->  |     HEADER    |   0x00008000  ----->  |               |
                    |_______________|                       |_______________| 

```
*Figure 1: Block overview.*

If both blocks are for some reason formatted correctly (power outage before the old block is erased), then the block count is used to determine the current block. The block count is increment every time there is a transfer between blocks.

Note: In this example, since the minimum write size to the flash is 32 bytes (a single page), the header will use the first page of each block.

### Writing Data

When writing data, a 4 bytes of extra data is needed to correct navigate and read data back (shown in Figure 2). First 2 bytes used is the virtual address. The virtual address is used to find specific data and overwrite specific sets of data (covered in a later section @TODO list actual section). The other 2 bytes is the length (in bytes) of the data being written. This is then used to transverse the current page buffer as well as the flash when searching for a particular virtual address.

```
                     _______________________________________________
0x0000003C  ----->  |           |           |           |           |
                    |_______________________________________________|
0x00000038  ----->  |           |           |           |           |
                    |_______________________________________________|
0x00000034  ----->  |           |           |           |           |
                    |_______________________________________________|
0x00000030  ----->  |           |           |           |           |
                    |_______________________________________________|
0x0000002C  ----->  |           |           |           |           |
                    |_______________________________________________|
0x00000028  ----->  |           |           |           |           |
                    |_______________________________________________|
0x00000024  ----->  |           |           |           |           |
                    |_______________________________________________|
0x00000020  ----->  |   VA_L    |   VA_H    |   LEN_L   |   LEN_H   |
                    |_______________________________________________|
    
```
*Figure 2: Header info for data about to be written.*

Once the header has been written, the data can be written (shown in Figure 3).

```
                     _______________________________________________
0x0000003C  ----->  |           |           |           |           |
                    |_______________________________________________|
0x00000038  ----->  |           |           |           |           |
                    |_______________________________________________|
0x00000034  ----->  |           |           |           |           |
                    |_______________________________________________|
0x00000030  ----->  |           |           |           |           |
                    |_______________________________________________|
0x0000002C  ----->  |   DATA    |   DATA    |           |           |
                    |_______________________________________________|
0x00000028  ----->  |   DATA    |   DATA    |   DATA    |   DATA    |
                    |_______________________________________________|
0x00000024  ----->  |   DATA    |   DATA    |   DATA    |   DATA    |
                    |_______________________________________________|
0x00000020  ----->  |   VA_L    |   VA_H    |   LEN_L   |   LEN_H   |
                    |_______________________________________________|
    
```
*Figure 3: View a full page.*

Each page saves the last 2 bytes of the buffer for a CRC to validate data when reading. The CRC is calculated from the data inside of the page buffer. Once the CRC is stored in the page buffer, the buffer is then written to the flash, shown in Figure 4.

```
                     _______________________________________________
0x0000003C  ----->  |   DATA    |   DATA    |   CRC_L   |   CRC_H   |
                    |_______________________________________________|
0x00000038  ----->  |   DATA    |   DATA    |   DATA    |   DATA    |
                    |_______________________________________________|
0x00000034  ----->  |   VA_L    |   VA_H    |   LEN_L   |   LEN_H   |
                    |_______________________________________________|
0x00000030  ----->  |   LEN_L   |   LEN_H   |   DATA    |   DATA    |
                    |_______________________________________________|
0x0000002C  ----->  |   DATA    |   DATA    |   VA_L    |   VA_H    |
                    |_______________________________________________|
0x00000028  ----->  |   DATA    |   DATA    |   DATA    |   DATA    |
                    |_______________________________________________|
0x00000024  ----->  |   DATA    |   DATA    |   DATA    |   DATA    |
                    |_______________________________________________|
0x00000020  ----->  |   VA_L    |   VA_H    |   LEN_L   |   LEN_H   |
                    |_______________________________________________|
    
```
*Figure 4: View a full page.*

Infomation about the current state of the emulated EEPROM is updated such as current page position (@TODO reference API struct) and the buffer is cleared and ready for next page of information.

### Writing Multiple Pages

The previous section covers a basic writing of data that fit perfectly in a single page. In most writes, this will not be the case. In the last write (at 0x00000034) in the previous figure, instead of having a length of 6 bytes, it will be 8 bytes in this example. This means that the last 2 bytes of the data will not be written in the same page as the rest of the data. 

Using the virtual address (100 in this example), each byte corresponds to the next incremental value of the virtual address. For example, the first of eight bytes would be stored at virtual address 100, the next byte at 101, and so on. Continuing with the example, this means the first six bytes would be stored between addresses 100-105. For the last two bytes on the new page, a virtual address of 106 will be written, with a length of 2, using virtual addresses 106 and 107 (shown in Figure 5).

```                 
                    |                      ...                      |
                    |_______________________________________________|
0x00000044  ----->  |   DATA    |   DATA    |           |           |
                    |_______________________________________________|
0x00000040  ----->  |   VA_L    |   VA_H    |   LEN_L   |   LEN_H   |
(Start of Page 3)   |_______________________________________________|

                     _______________________________________________
0x0000003C  ----->  |   DATA    |   DATA    |   CRC_L   |   CRC_H   | \
(End of Page 2)     |_______________________________________________|  \
0x00000038  ----->  |   DATA    |   DATA    |   DATA    |   DATA    |   \
                    |_______________________________________________|    \
0x00000034  ----->  |   VA_L    |   VA_H    |   LEN_L   |   LEN_H   |     \ Written to flash
                    |_______________________________________________|     /
                    |                      ...                      |    /
```
*Figure 5: Data spread across multiple pages.*

### Full Block/Transferring Between Blocks

When a block becomes full, the latest of the data in the full block is transferred over to the new block. This is done by reading the currently full block from newest to old. Doing this allows the newest data of each stored virtual address to be moved to the newest block. As data associated with each virtual address is moved, and bitmap tracks which virtual address data has been moved to avoid duplicates.  

### Erasing Data

To erase data, the virtual address, associated with that data, is written to the EEPROM with a size of zero. The transfer function will check the bitmap to see if the virtual address has already been transferred. If it has not, the transfer function will mark it as transferred.