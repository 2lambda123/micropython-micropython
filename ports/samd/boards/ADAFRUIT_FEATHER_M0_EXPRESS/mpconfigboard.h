#define MICROPY_HW_BOARD_NAME "Feather M0 Express"
#define MICROPY_HW_MCU_NAME   "SAMD21G18A"

// MicroPython configs
// samd_flash.c flash parameters
// Build a 64k Flash storage at top. 256k-64k=196k
// 256*1024=262144 minus 64*1024=65536 = 196608 = 0x30000
#define MICROPY_HW_FLASH_STORAGE_BASE       (0x30000)
#define MICROPY_HW_FLASH_STORAGE_BYTES      (0xFFFF) 
#define VFS_BLOCK_SIZE_BYTES                (1536) // 24x 64B flash pages;

// ASF4 MCU package specific Pin definitions
#include "samd21g18a.h"

// Please consult the SAM_D21 Datasheet, I/O Multiplexing and Considerations.
// USART pin assignments: Tx=PB22=SERCOM5/PAD[2], Rx=PB23==SERCOM5/PAD[3]
#define CPU_FREQ (48000000) // For selecting Baud from clock.
#define MP_PIN_GRP 1 // A=0, B=1
#define MP_TX_PIN 22 //'n'
#define MP_RX_PIN 23
#define MP_PERIPHERAL_MUX 11 // 'n'th group of 2 pins
#define USARTx SERCOM5 // SERCOM4:XIAO_M0 tx/rx
#define MP_PORT_FUNC 0x33 // Sets PMUXE & PMUXO to the Alternative Function.A-H=0-7
#define MP_RXPO_PAD 3 // RXPO- Receive Data Pinout
#define MP_TXPO_PAD 2 // TXPO- Tranmit Data Pinout
#define MP_SERCOMx SERCOM5_ // APBCMASK
#define MP_SERCOM_GCLK_ID_x_CORE GCLK_CLKCTRL_ID_SERCOM5_CORE // Generic Clock Control
