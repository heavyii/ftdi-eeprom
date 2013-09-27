

#CROSS_COMPILE ?= arm-none-linux-gnueabi-
#CROSS_COMPILE ?= arm-arago-linux-gnueabi-
#CROSS_COMPILE ?= arm-linux-
CC=$(CROSS_COMPILE)gcc

CFLAGS := -O1 -g -Wall -pipe  -Wno-unused-result -Wno-pointer-sign

APP = ftdi-eeprom
all: $(APP)

.c.o:
	@$(CC)  $(CFLAGS) $(INCLUDES)   -c $< -o $@

# if program halt, add -lpthread
ftdi-eeprom_obj = ftdi_eeprom.o 
ftdi-eeprom:$(ftdi-eeprom_obj)
	@$(CC) $^ -o $@ -lftdi1 -lusb-1.0 

clean:
	@rm -rf $(APP) *.o
.PHONY: clean 
