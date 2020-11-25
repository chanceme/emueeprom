IDIR=../inc 
CC=gcc
CFLAGS=-I$(IDIR) -Wall -DLINUX 
DEPS = flash.h flash_config.h emueeprom.h test.h
OBJ = main.o flash.o emueeprom.o test.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

emueeprom: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)