

ftdi-eeprom
===========

A tool to read & write EEPROM of ft232 chips, and demonstrate how to use empty space in ftdi chip's EEPROM for personal usage.

Copyright (C) 2013 ruisheng <ruishengleen@gmail.com>
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	usage: ./ftdi-eeprom [options]
	   -s
		  usb device serial string
	   -g
		  get id
	   -i [id]
		  set id
	   -r [file]
		  read device's eeprom to file
	   -w [file]
		  write device's eeprom to file
	   -S [serial string]
		  set new serial string
	   -l
		  list ftdi devices
	   -v
		  verbose output
	   -h
		  help

	examples:
	read ID:
		./ftdi-eeprom -s [serial] -g
	write ID:
		./ftdi-eeprom -s [serial] -i [ID]
	read device's EEPROM to file:
		./ftdi-eeprom -s [serial] -r [EEPROM file]
	write EEPROM file to device's EEPROM, only change serial string:
		./ftdi-eeprom -s [serial] -S [new serial] -w [EEPROM file]
