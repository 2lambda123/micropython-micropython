#define MICROPY_HW_BOARD_NAME       "NUCLEO-L432KC_LFS1"
#define MICROPY_HW_MCU_NAME         "STM32L432KC"

#define MICROPY_EMIT_THUMB          (0)
#define MICROPY_EMIT_INLINE_THUMB   (0)
#define MICROPY_PY_BUILTINS_COMPLEX (0)
#define MICROPY_PY_GENERATOR_PEND_THROW (0)
#define MICROPY_PY_FRAMEBUF         (1) // 4KB.
#define MICROPY_PY_USOCKET          (0)
#define MICROPY_PY_NETWORK          (0)
#define MICROPY_PY_ONEWIRE          (0)
#define MICROPY_PY_STM              (0)
#define MICROPY_PY_PYB_LEGACY       (0)
#define MICROPY_PY_UHEAPQ           (0)
#define MICROPY_PY_UTIMEQ           (0)

#define MICROPY_HW_HAS_FLASH        (1)
#define MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE (1)
#define MICROPY_HW_ENABLE_RTC       (1)
#define MICROPY_HW_ENABLE_ADC       (1)
#define MICROPY_HW_ENABLE_DAC       (1)
#define MICROPY_HW_ENABLE_USB       (0) // requires a custom USB cable on PA11 & PA12. Approx 10KB.
#define MICROPY_HW_ENABLE_TIMER     (1)
#define MICROPY_HW_HAS_SWITCH       (0)
#define MICROPY_HW_ENABLE_CAN       (0) // Approx 6KB.

// MSI is used and is 4MHz
#define MICROPY_HW_CLK_PLLM (1)
#define MICROPY_HW_CLK_PLLN (16) // Change to 40 for 80MHz.
#define MICROPY_HW_CLK_PLLR (RCC_PLLR_DIV2) // SYSCLK = 4*1*16/2 = 32MHz.
#define MICROPY_HW_CLK_PLLP (RCC_PLLP_DIV7)
#define MICROPY_HW_CLK_PLLQ (RCC_PLLQ_DIV2)
#define MICROPY_HW_FLASH_LATENCY    FLASH_LATENCY_4

// The board has an external 32kHz crystal
#define MICROPY_HW_RTC_USE_LSE      (1)

// UART config
#define MICROPY_HW_UART1_TX     (pin_B6)
#define MICROPY_HW_UART1_RX     (pin_B7)
#define MICROPY_HW_UART2_TX     (pin_A2)  // VCP TX
#define MICROPY_HW_UART2_RX     (pin_A15) // VCP RX

#define MICROPY_HW_UART_REPL        PYB_UART_2
#define MICROPY_HW_UART_REPL_BAUD   115200

// I2C busses
#define MICROPY_HW_I2C1_SCL (pin_A9)
#define MICROPY_HW_I2C1_SDA (pin_A10)
#define MICROPY_HW_I2C3_SCL (pin_A7)
#define MICROPY_HW_I2C3_SDA (pin_B4)

// SPI busses
#define MICROPY_HW_SPI1_NSS     (pin_B0)
#define MICROPY_HW_SPI1_SCK     (pin_A5)
#define MICROPY_HW_SPI1_MISO    (pin_A6)
#define MICROPY_HW_SPI1_MOSI    (pin_A7)
#define MICROPY_HW_SPI3_NSS     (pin_A4)
#define MICROPY_HW_SPI3_SCK     (pin_B3)
#define MICROPY_HW_SPI3_MISO    (pin_B4)
#define MICROPY_HW_SPI3_MOSI    (pin_B5)

// LEDs
#define MICROPY_HW_LED1             (pin_B3) // Green LD3 LED on Nucleo
#define MICROPY_HW_LED_ON(pin)      (mp_hal_pin_high(pin))
#define MICROPY_HW_LED_OFF(pin)     (mp_hal_pin_low(pin))

// USB config
#define MICROPY_HW_USB_FS           (MICROPY_HW_ENABLE_USB)
#define MICROPY_HW_USB_MSC          (0)
#define MICROPY_HW_USB_HID          (0)
#define USBD_CDC_RX_DATA_SIZE       (256)
#define USBD_CDC_TX_DATA_SIZE       (256)
