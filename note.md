FT232 EEPROM  note
==================

FT232 usb configuration is stored in eeprom， [libftdi-1.0][libftdi-web] can read&write eeprom。
Libftdi-0.2 use libusb-0.1， libftdi-1.0 use libusb-1.0。

[libftdi-web]: http://www.intra2net.com/en/developer/libftdi/download.php "libftdi website"

Why libftdi-1.0
---------------

* Ftdi_sio driver

I neet tty device, after EEPROM operations, libusb_attach_kernel_driver（） can attach ftdi_sio.
	
* Replug device

Let kernel reread device's information.
	
	libusb_reset_device()
	
	The system will attempt to restore the previous configuration and alternate settings after the reset has completed.
	If the reset fails, the descriptors change, or the previous state cannot be restored, the device will appear to be disconnected and reconnected. This means that the device handle is no longer valid (you should close it) and rediscover the device. A return code of LIBUSB_ERROR_NOT_FOUND indicates when this is the case.
	This is a blocking function which usually incurs a noticeable delay.


FT232 EEPROM
------------

* size

	ft232h： 256 0x100
	
	ft232bm: 128 0x80
	
* usage at end end of EEPROM： 

	manufacturer[n] + product[n] + serial[n] + port_pnp[3] + unused[n] + checksum[2]
	
It's very easy to change those string informations.


Cross compile libusb-0.1.12
---------------------------

	CC=arm-none-linux-gnueabi-gcc CXX=arm-none-linux-gnueabi-g++ ./configure --host=arm-none-linux-gnueabi --build=i386 --without-pkgconfig –disable-build-docs –prefix=/usr/local/arm/3.4/
	make && make install


Cross compile libftdi-1.0
-------------------------

Set cross tool chain and libusb-1.0 directory

* Toolchain-Crossbuild32.cmake

```
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_C_COMPILER arm-linux-gcc)
set(CMAKE_CXX_COMPILER arm-linux-g++)
set(CMAKE_FIND_ROOT_PATH /usr/local/arm/3.4/lib)

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search 
# programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# libusb-1.0
set(LIBUSB_INCLUDE_DIR /usr/local/arm/3.4/arm-linux/sys-include/libusb-1.0)
set(LIBUSB_LIBRARIES /usr/local/arm/3.4/arm-linux/lib/libusb-1.0.so.0.0.0)
```

Don't compile ftdi-eeprom，because I don't have libconfuse。

* CMakeLists.txt

```
add_subdirectory(src)
add_subdirectory(ftdipp)
add_subdirectory(bindings)
#add_subdirectory(ftdi_eeprom)
add_subdirectory(examples)
add_subdirectory(packages)
add_subdirectory(test)
```

compile
-------

	mkdir build
	cd build
	cmake -DCMAKE_TOOLCHAIN_FILE=../Toolchain-Crossbuild32.cmake ..


my toolchain's bug?
-------------------

I compile program with -lftdi1 -lusb-1.0 in ubuntu, all works fine. 
But program stop at libusb function when running on my arm board. Finally I try to compile it with -lpthread, it works!!!

	ftdi-eeprom_obj = ftdi_eeprom.o 
	ftdi-eeprom:$(ftdi-eeprom_obj)
		@(CC) $^ -o $@ -lftdi1 -lusb-1.0 -lpthread
