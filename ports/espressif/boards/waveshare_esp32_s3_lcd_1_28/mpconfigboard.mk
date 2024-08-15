CIRCUITPY_CREATOR_ID = 0x1BBB0000
CIRCUITPY_CREATION_ID = 0x00AB0001

# This board doesn't have USB by default, it
# instead uses a CH340C USB-to-Serial chip
CIRCUITPY_USB_DEVICE = 0
CIRCUITPY_ESP_USB_SERIAL_JTAG = 0

IDF_TARGET = esp32s3

# This flash lives outside the module.
CIRCUITPY_ESP_FLASH_MODE = qio
CIRCUITPY_ESP_FLASH_FREQ = 80m
CIRCUITPY_ESP_FLASH_SIZE = 16MB

CIRCUITPY_ESP_PSRAM_SIZE = 2MB
CIRCUITPY_ESP_PSRAM_MODE = qio
CIRCUITPY_ESP_PSRAM_FREQ = 80m

INTERNAL_FLASH_FILESYSTEM = 0
QSPI_FLASH_FILESYSTEM = 1
EXTERNAL_FLASH_DEVICES = W25Q128JVxQ

CIRCUITPY_ESPCAMERA = 0