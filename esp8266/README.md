MicroPython port to ESP8266
===========================

This is a highly experimental port of MicroPython for the WiFi modules based
on Espressif ESP8266 chip.

WARNING: The port is highly experimental and any APIs are subject to change.

Currently implemented features include:
- REPL (Python prompt) over UART0.
- Garbage collector, exceptions.
- Unicode support.
- Builtin modules: gc, array, collections, io, struct, sys, esp, network.
- Arbitrary-precision long integers and 30-bit precision floats.
- Basic WiFi support.
- Basic sockets using modlwip.
- Basic GPIO support.

On the TODO list:
- Full wifi support.
- Internal filesystem using the flash.
- ...

Build instructions
------------------

The tool chain required for the build is the OpenSource ESP SDK, which can be
found at <https://github.com/pfalcon/esp-open-sdk>.  Clone this repository and
run `make` in its directory to build and install the SDK locally.  Make sure
to add toolchain bin directory to your PATH.

Add the external dependencies to the MicroPython repository checkout:
```bash
$ git submodule update --init
```
See the README in the repository root for more information about external
dependencies.

Then, to build MicroPython for the ESP8266, just run:
```bash
$ cd esp8266
$ make
```
This should produce binary images in the `build/` subdirectory.  To flash them
to your ESP8266, use:
```bash
$ make deploy
```
This will use the `esptool.py` script to download the images.  You must have
your ESP module in the bootloader, and connected to a serial port on your PC.
The default serial port is `/dev/ttyACM0`.  To specify another, use, eg:
```bash
$ make PORT=/dev/ttyUSB0 deploy
```

The images that are built are:
- `firmware.elf-0x00000.bin`: to be flashed at 0x00000
- `firmware.elf-0x10000.bin`: to be flashed at 0x10000

There is also a combined image, made up of the above 2 binary files with the
appropriate padding:
- `firmware-combined.bin`: to be flashed at 0x00000
