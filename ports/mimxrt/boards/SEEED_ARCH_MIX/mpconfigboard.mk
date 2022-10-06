MCU_SERIES = MIMXRT1052
MCU_VARIANT = MIMXRT1052DVL6B

MICROPY_FLOAT_IMPL = double
MICROPY_PY_MACHINE_SDCARD = 1
MICROPY_HW_FLASH_TYPE = qspi_nor_flash
MICROPY_HW_FLASH_SIZE = 0x800000  # 8MB
	
MICROPY_HW_SDRAM_AVAIL = 1
MICROPY_HW_SDRAM_SIZE  = 0x2000000  # 32MB

MICROPY_BOOT_BUFFER_SIZE = (64 * 1024)
MICROPY_PY_LWIP = 1
MICROPY_PY_USSL = 1
MICROPY_SSL_MBEDTLS = 1
