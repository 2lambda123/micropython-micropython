MCU_SERIES = m4
MCU_VARIANT = nrf52
MCU_SUB_VARIANT = nrf52832
SOFTDEV_VERSION = 6.0.0
LD_FILES += boards/nrf52832_512k_64k.ld

NRF_DEFINES += -DNRF52832_XXAA
CFLAGS += -DBLUETOOTH_LFCLK_RC
