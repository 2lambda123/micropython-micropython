/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "py/runtime.h"
#include "py/stream.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "lib/utils/interrupt_char.h"
#include "lib/utils/mpirq.h"
#include "uart.h"
#include "irq.h"
#include "pendsv.h"

#if defined(STM32F4)
#define UART_RXNE_IS_SET(uart) ((uart)->SR & USART_SR_RXNE)
#elif defined(STM32H7)
#define UART_RXNE_IS_SET(uart) ((uart)->ISR & USART_ISR_RXNE_RXFNE)
#else
#define UART_RXNE_IS_SET(uart) ((uart)->ISR & USART_ISR_RXNE)
#endif
#define UART_RXNE_IT_EN(uart) do {(uart)->CR1 |= USART_CR1_RXNEIE;} while (0)
#define UART_RXNE_IT_DIS(uart) do {(uart)->CR1 &= ~USART_CR1_RXNEIE;} while (0)

#define USART_CR1_IE_BASE (USART_CR1_PEIE | USART_CR1_TXEIE | USART_CR1_TCIE | USART_CR1_RXNEIE | USART_CR1_IDLEIE)
#define USART_CR2_IE_BASE (USART_CR2_LBDIE)
#define USART_CR3_IE_BASE (USART_CR3_CTSIE | USART_CR3_EIE)

#if defined(STM32F0)
#define USART_CR1_IE_ALL (USART_CR1_IE_BASE | USART_CR1_EOBIE | USART_CR1_RTOIE | USART_CR1_CMIE)
#define USART_CR2_IE_ALL (USART_CR2_IE_BASE)
#define USART_CR3_IE_ALL (USART_CR3_IE_BASE | USART_CR3_WUFIE)

#elif defined(STM32F4)
#define USART_CR1_IE_ALL (USART_CR1_IE_BASE)
#define USART_CR2_IE_ALL (USART_CR2_IE_BASE)
#define USART_CR3_IE_ALL (USART_CR3_IE_BASE)

#elif defined(STM32F7)
#define USART_CR1_IE_ALL (USART_CR1_IE_BASE | USART_CR1_EOBIE | USART_CR1_RTOIE | USART_CR1_CMIE)
#define USART_CR2_IE_ALL (USART_CR2_IE_BASE)
#if defined(USART_CR3_TCBGTIE)
#define USART_CR3_IE_ALL (USART_CR3_IE_BASE | USART_CR3_TCBGTIE)
#else
#define USART_CR3_IE_ALL (USART_CR3_IE_BASE)
#endif

#elif defined(STM32H7)
#define USART_CR1_IE_ALL (USART_CR1_IE_BASE | USART_CR1_RXFFIE | USART_CR1_TXFEIE | USART_CR1_EOBIE | USART_CR1_RTOIE | USART_CR1_CMIE)
#define USART_CR2_IE_ALL (USART_CR2_IE_BASE)
#define USART_CR3_IE_ALL (USART_CR3_IE_BASE | USART_CR3_RXFTIE | USART_CR3_TCBGTIE | USART_CR3_TXFTIE | USART_CR3_WUFIE)

#elif defined(STM32L0)
#define USART_CR1_IE_ALL (USART_CR1_IE_BASE | USART_CR1_EOBIE | USART_CR1_RTOIE | USART_CR1_CMIE)
#define USART_CR2_IE_ALL (USART_CR2_IE_BASE)
#define USART_CR3_IE_ALL (USART_CR3_IE_BASE | USART_CR3_WUFIE)

#elif defined(STM32L4) || defined(STM32WB)
#define USART_CR1_IE_ALL (USART_CR1_IE_BASE | USART_CR1_EOBIE | USART_CR1_RTOIE | USART_CR1_CMIE)
#define USART_CR2_IE_ALL (USART_CR2_IE_BASE)
#if defined(USART_CR3_TCBGTIE)
#define USART_CR3_IE_ALL (USART_CR3_IE_BASE | USART_CR3_TCBGTIE | USART_CR3_WUFIE)
#else
#define USART_CR3_IE_ALL (USART_CR3_IE_BASE | USART_CR3_WUFIE)
#endif
#endif

extern void NORETURN __fatal_error(const char *msg);

void uart_init0(void) {
    #if defined(STM32H7)
    RCC_PeriphCLKInitTypeDef RCC_PeriphClkInit = {0};
    // Configure USART1/6 clock source
    RCC_PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART16;
    RCC_PeriphClkInit.Usart16ClockSelection = RCC_USART16CLKSOURCE_D2PCLK2;
    if (HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphClkInit) != HAL_OK) {
        __fatal_error("HAL_RCCEx_PeriphCLKConfig");
    }

    // Configure USART2/3/4/5/7/8 clock source
    RCC_PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART234578;
    RCC_PeriphClkInit.Usart16ClockSelection = RCC_USART234578CLKSOURCE_D2PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphClkInit) != HAL_OK) {
        __fatal_error("HAL_RCCEx_PeriphCLKConfig");
    }
    #endif
}

// unregister all interrupt sources
void uart_deinit_all(void) {
    for (int i = 0; i < MP_ARRAY_SIZE(MP_STATE_PORT(pyb_uart_obj_all)); i++) {
        pyb_uart_obj_t *uart_obj = MP_STATE_PORT(pyb_uart_obj_all)[i];
        if (uart_obj != NULL && !uart_obj->is_static) {
            uart_deinit(uart_obj);
            MP_STATE_PORT(pyb_uart_obj_all)[i] = NULL;
        }
    }
}

bool uart_exists(int uart_id) {
    if (uart_id > MP_ARRAY_SIZE(MP_STATE_PORT(pyb_uart_obj_all))) {
        // safeguard against pyb_uart_obj_all array being configured too small
        return false;
    }
    switch (uart_id) {
        #if defined(MICROPY_HW_UART1_TX) && defined(MICROPY_HW_UART1_RX)
        case PYB_UART_1:
            return true;
        #endif

        #if defined(MICROPY_HW_UART2_TX) && defined(MICROPY_HW_UART2_RX)
        case PYB_UART_2:
            return true;
        #endif

        #if defined(MICROPY_HW_UART3_TX) && defined(MICROPY_HW_UART3_RX)
        case PYB_UART_3:
            return true;
        #endif

        #if defined(MICROPY_HW_UART4_TX) && defined(MICROPY_HW_UART4_RX)
        case PYB_UART_4:
            return true;
        #endif

        #if defined(MICROPY_HW_UART5_TX) && defined(MICROPY_HW_UART5_RX)
        case PYB_UART_5:
            return true;
        #endif

        #if defined(MICROPY_HW_UART6_TX) && defined(MICROPY_HW_UART6_RX)
        case PYB_UART_6:
            return true;
        #endif

        #if defined(MICROPY_HW_UART7_TX) && defined(MICROPY_HW_UART7_RX)
        case PYB_UART_7:
            return true;
        #endif

        #if defined(MICROPY_HW_UART8_TX) && defined(MICROPY_HW_UART8_RX)
        case PYB_UART_8:
            return true;
        #endif

        #if defined(MICROPY_HW_UART9_TX) && defined(MICROPY_HW_UART9_RX)
        case PYB_UART_9:
            return true;
        #endif

        #if defined(MICROPY_HW_UART10_TX) && defined(MICROPY_HW_UART10_RX)
        case PYB_UART_10:
            return true;
        #endif

        default:
            return false;
    }
}

// assumes Init parameters have been set up correctly
bool uart_init(pyb_uart_obj_t *uart_obj,
    uint32_t baudrate, uint32_t bits, uint32_t parity, uint32_t stop, uint32_t flow) {
    USART_TypeDef *UARTx;
    IRQn_Type irqn;
    int uart_unit;

    const pin_obj_t *pins[4] = {0};

    switch (uart_obj->uart_id) {
        #if defined(MICROPY_HW_UART1_TX) && defined(MICROPY_HW_UART1_RX)
        case PYB_UART_1:
            uart_unit = 1;
            UARTx = USART1;
            irqn = USART1_IRQn;
            pins[0] = MICROPY_HW_UART1_TX;
            pins[1] = MICROPY_HW_UART1_RX;
            __HAL_RCC_USART1_CLK_ENABLE();
            break;
        #endif

        #if defined(MICROPY_HW_UART2_TX) && defined(MICROPY_HW_UART2_RX)
        case PYB_UART_2:
            uart_unit = 2;
            UARTx = USART2;
            irqn = USART2_IRQn;
            pins[0] = MICROPY_HW_UART2_TX;
            pins[1] = MICROPY_HW_UART2_RX;
            #if defined(MICROPY_HW_UART2_RTS)
            if (flow & UART_HWCONTROL_RTS) {
                pins[2] = MICROPY_HW_UART2_RTS;
            }
            #endif
            #if defined(MICROPY_HW_UART2_CTS)
            if (flow & UART_HWCONTROL_CTS) {
                pins[3] = MICROPY_HW_UART2_CTS;
            }
            #endif
            __HAL_RCC_USART2_CLK_ENABLE();
            break;
        #endif

        #if defined(MICROPY_HW_UART3_TX) && defined(MICROPY_HW_UART3_RX)
        case PYB_UART_3:
            uart_unit = 3;
            UARTx = USART3;
            #if defined(STM32F0)
            irqn = USART3_8_IRQn;
            #else
            irqn = USART3_IRQn;
            #endif
            pins[0] = MICROPY_HW_UART3_TX;
            pins[1] = MICROPY_HW_UART3_RX;
            #if defined(MICROPY_HW_UART3_RTS)
            if (flow & UART_HWCONTROL_RTS) {
                pins[2] = MICROPY_HW_UART3_RTS;
            }
            #endif
            #if defined(MICROPY_HW_UART3_CTS)
            if (flow & UART_HWCONTROL_CTS) {
                pins[3] = MICROPY_HW_UART3_CTS;
            }
            #endif
            __HAL_RCC_USART3_CLK_ENABLE();
            break;
        #endif

        #if defined(MICROPY_HW_UART4_TX) && defined(MICROPY_HW_UART4_RX)
        case PYB_UART_4:
            uart_unit = 4;
            #if defined(STM32F0)
            UARTx = USART4;
            irqn = USART3_8_IRQn;
            __HAL_RCC_USART4_CLK_ENABLE();
            #elif defined(STM32L0)
            UARTx = USART4;
            irqn = USART4_5_IRQn;
            __HAL_RCC_USART4_CLK_ENABLE();
            #else
            UARTx = UART4;
            irqn = UART4_IRQn;
            __HAL_RCC_UART4_CLK_ENABLE();
            #endif
            pins[0] = MICROPY_HW_UART4_TX;
            pins[1] = MICROPY_HW_UART4_RX;
            #if defined(MICROPY_HW_UART4_RTS)
            if (flow & UART_HWCONTROL_RTS) {
                pins[2] = MICROPY_HW_UART4_RTS;
            }
            #endif
            #if defined(MICROPY_HW_UART4_CTS)
            if (flow & UART_HWCONTROL_CTS) {
                pins[3] = MICROPY_HW_UART4_CTS;
            }
            #endif
            break;
        #endif

        #if defined(MICROPY_HW_UART5_TX) && defined(MICROPY_HW_UART5_RX)
        case PYB_UART_5:
            uart_unit = 5;
            #if defined(STM32F0)
            UARTx = USART5;
            irqn = USART3_8_IRQn;
            __HAL_RCC_USART5_CLK_ENABLE();
            #elif defined(STM32L0)
            UARTx = USART5;
            irqn = USART4_5_IRQn;
            __HAL_RCC_USART5_CLK_ENABLE();
            #else
            UARTx = UART5;
            irqn = UART5_IRQn;
            __HAL_RCC_UART5_CLK_ENABLE();
            #endif
            pins[0] = MICROPY_HW_UART5_TX;
            pins[1] = MICROPY_HW_UART5_RX;
            break;
        #endif

        #if defined(MICROPY_HW_UART6_TX) && defined(MICROPY_HW_UART6_RX)
        case PYB_UART_6:
            uart_unit = 6;
            UARTx = USART6;
            #if defined(STM32F0)
            irqn = USART3_8_IRQn;
            #else
            irqn = USART6_IRQn;
            #endif
            pins[0] = MICROPY_HW_UART6_TX;
            pins[1] = MICROPY_HW_UART6_RX;
            #if defined(MICROPY_HW_UART6_RTS)
            if (flow & UART_HWCONTROL_RTS) {
                pins[2] = MICROPY_HW_UART6_RTS;
            }
            #endif
            #if defined(MICROPY_HW_UART6_CTS)
            if (flow & UART_HWCONTROL_CTS) {
                pins[3] = MICROPY_HW_UART6_CTS;
            }
            #endif
            __HAL_RCC_USART6_CLK_ENABLE();
            break;
        #endif

        #if defined(MICROPY_HW_UART7_TX) && defined(MICROPY_HW_UART7_RX)
        case PYB_UART_7:
            uart_unit = 7;
            #if defined(STM32F0)
            UARTx = USART7;
            irqn = USART3_8_IRQn;
            __HAL_RCC_USART7_CLK_ENABLE();
            #else
            UARTx = UART7;
            irqn = UART7_IRQn;
            __HAL_RCC_UART7_CLK_ENABLE();
            #endif
            pins[0] = MICROPY_HW_UART7_TX;
            pins[1] = MICROPY_HW_UART7_RX;
            break;
        #endif

        #if defined(MICROPY_HW_UART8_TX) && defined(MICROPY_HW_UART8_RX)
        case PYB_UART_8:
            uart_unit = 8;
            #if defined(STM32F0)
            UARTx = USART8;
            irqn = USART3_8_IRQn;
            __HAL_RCC_USART8_CLK_ENABLE();
            #else
            UARTx = UART8;
            irqn = UART8_IRQn;
            __HAL_RCC_UART8_CLK_ENABLE();
            #endif
            pins[0] = MICROPY_HW_UART8_TX;
            pins[1] = MICROPY_HW_UART8_RX;
            break;
        #endif

        #if defined(MICROPY_HW_UART9_TX) && defined(MICROPY_HW_UART9_RX)
        case PYB_UART_9:
            uart_unit = 9;
            UARTx = UART9;
            irqn = UART9_IRQn;
            __HAL_RCC_UART9_CLK_ENABLE();
            pins[0] = MICROPY_HW_UART9_TX;
            pins[1] = MICROPY_HW_UART9_RX;
            break;
        #endif

        #if defined(MICROPY_HW_UART10_TX) && defined(MICROPY_HW_UART10_RX)
        case PYB_UART_10:
            uart_unit = 10;
            UARTx = UART10;
            irqn = UART10_IRQn;
            __HAL_RCC_UART10_CLK_ENABLE();
            pins[0] = MICROPY_HW_UART10_TX;
            pins[1] = MICROPY_HW_UART10_RX;
            break;
        #endif

        default:
            // UART does not exist or is not configured for this board
            return false;
    }

    uint32_t mode = MP_HAL_PIN_MODE_ALT;
    uint32_t pull = MP_HAL_PIN_PULL_UP;

    for (uint i = 0; i < 4; i++) {
        if (pins[i] != NULL) {
            bool ret = mp_hal_pin_config_alt(pins[i], mode, pull, AF_FN_UART, uart_unit);
            if (!ret) {
                return false;
            }
        }
    }

    uart_obj->uartx = UARTx;

    // init UARTx
    UART_HandleTypeDef huart;
    memset(&huart, 0, sizeof(huart));
    huart.Instance = UARTx;
    huart.Init.BaudRate = baudrate;
    huart.Init.WordLength = bits;
    huart.Init.StopBits = stop;
    huart.Init.Parity = parity;
    huart.Init.Mode = UART_MODE_TX_RX;
    huart.Init.HwFlowCtl = flow;
    huart.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart);

    // Disable all individual UART IRQs, but enable the global handler
    uart_obj->uartx->CR1 &= ~USART_CR1_IE_ALL;
    uart_obj->uartx->CR2 &= ~USART_CR2_IE_ALL;
    uart_obj->uartx->CR3 &= ~USART_CR3_IE_ALL;
    NVIC_SetPriority(IRQn_NONNEG(irqn), IRQ_PRI_UART);
    HAL_NVIC_EnableIRQ(irqn);

    uart_obj->is_enabled = true;
    uart_obj->attached_to_repl = false;

    if (bits == UART_WORDLENGTH_9B && parity == UART_PARITY_NONE) {
        uart_obj->char_mask = 0x1ff;
        uart_obj->char_width = CHAR_WIDTH_9BIT;
    } else {
        if (bits == UART_WORDLENGTH_9B || parity == UART_PARITY_NONE) {
            uart_obj->char_mask = 0xff;
        } else {
            uart_obj->char_mask = 0x7f;
        }
        uart_obj->char_width = CHAR_WIDTH_8BIT;
    }

    uart_obj->mp_irq_trigger = 0;
    uart_obj->mp_irq_obj = NULL;

    return true;
}

void uart_set_rxbuf(pyb_uart_obj_t *self, size_t len, void *buf) {
    self->read_buf_head = 0;
    self->read_buf_tail = 0;
    self->read_buf_len = len;
    self->read_buf = buf;
    if (len == 0) {
        UART_RXNE_IT_DIS(self->uartx);
    } else {
        UART_RXNE_IT_EN(self->uartx);
    }
}

void uart_deinit(pyb_uart_obj_t *self) {
    self->is_enabled = false;

    // Disable UART
    self->uartx->CR1 &= ~USART_CR1_UE;

    // Reset and turn off the UART peripheral
    if (self->uart_id == 1) {
        HAL_NVIC_DisableIRQ(USART1_IRQn);
        __HAL_RCC_USART1_FORCE_RESET();
        __HAL_RCC_USART1_RELEASE_RESET();
        __HAL_RCC_USART1_CLK_DISABLE();
    #if defined(USART2)
    } else if (self->uart_id == 2) {
        HAL_NVIC_DisableIRQ(USART2_IRQn);
        __HAL_RCC_USART2_FORCE_RESET();
        __HAL_RCC_USART2_RELEASE_RESET();
        __HAL_RCC_USART2_CLK_DISABLE();
    #endif
    #if defined(USART3)
    } else if (self->uart_id == 3) {
        #if !defined(STM32F0)
        HAL_NVIC_DisableIRQ(USART3_IRQn);
        #endif
        __HAL_RCC_USART3_FORCE_RESET();
        __HAL_RCC_USART3_RELEASE_RESET();
        __HAL_RCC_USART3_CLK_DISABLE();
    #endif
    #if defined(UART4)
    } else if (self->uart_id == 4) {
        HAL_NVIC_DisableIRQ(UART4_IRQn);
        __HAL_RCC_UART4_FORCE_RESET();
        __HAL_RCC_UART4_RELEASE_RESET();
        __HAL_RCC_UART4_CLK_DISABLE();
    #endif
    #if defined(USART4)
    } else if (self->uart_id == 4) {
        __HAL_RCC_USART4_FORCE_RESET();
        __HAL_RCC_USART4_RELEASE_RESET();
        __HAL_RCC_USART4_CLK_DISABLE();
    #endif
    #if defined(UART5)
    } else if (self->uart_id == 5) {
        HAL_NVIC_DisableIRQ(UART5_IRQn);
        __HAL_RCC_UART5_FORCE_RESET();
        __HAL_RCC_UART5_RELEASE_RESET();
        __HAL_RCC_UART5_CLK_DISABLE();
    #endif
    #if defined(USART5)
    } else if (self->uart_id == 5) {
        __HAL_RCC_USART5_FORCE_RESET();
        __HAL_RCC_USART5_RELEASE_RESET();
        __HAL_RCC_USART5_CLK_DISABLE();
    #endif
    #if defined(UART6)
    } else if (self->uart_id == 6) {
        HAL_NVIC_DisableIRQ(USART6_IRQn);
        __HAL_RCC_USART6_FORCE_RESET();
        __HAL_RCC_USART6_RELEASE_RESET();
        __HAL_RCC_USART6_CLK_DISABLE();
    #endif
    #if defined(UART7)
    } else if (self->uart_id == 7) {
        HAL_NVIC_DisableIRQ(UART7_IRQn);
        __HAL_RCC_UART7_FORCE_RESET();
        __HAL_RCC_UART7_RELEASE_RESET();
        __HAL_RCC_UART7_CLK_DISABLE();
    #endif
    #if defined(USART7)
    } else if (self->uart_id == 7) {
        __HAL_RCC_USART7_FORCE_RESET();
        __HAL_RCC_USART7_RELEASE_RESET();
        __HAL_RCC_USART7_CLK_DISABLE();
    #endif
    #if defined(UART8)
    } else if (self->uart_id == 8) {
        HAL_NVIC_DisableIRQ(UART8_IRQn);
        __HAL_RCC_UART8_FORCE_RESET();
        __HAL_RCC_UART8_RELEASE_RESET();
        __HAL_RCC_UART8_CLK_DISABLE();
    #endif
    #if defined(USART8)
    } else if (self->uart_id == 8) {
        __HAL_RCC_USART8_FORCE_RESET();
        __HAL_RCC_USART8_RELEASE_RESET();
        __HAL_RCC_USART8_CLK_DISABLE();
    #endif
    #if defined(UART9)
    } else if (self->uart_id == 9) {
        HAL_NVIC_DisableIRQ(UART9_IRQn);
        __HAL_RCC_UART9_FORCE_RESET();
        __HAL_RCC_UART9_RELEASE_RESET();
        __HAL_RCC_UART9_CLK_DISABLE();
    #endif
    #if defined(UART10)
    } else if (self->uart_id == 10) {
        HAL_NVIC_DisableIRQ(UART10_IRQn);
        __HAL_RCC_UART10_FORCE_RESET();
        __HAL_RCC_UART10_RELEASE_RESET();
        __HAL_RCC_UART10_CLK_DISABLE();
    #endif
    }
}

void uart_attach_to_repl(pyb_uart_obj_t *self, bool attached) {
    self->attached_to_repl = attached;
}

uint32_t uart_get_baudrate(pyb_uart_obj_t *self) {
    uint32_t uart_clk = 0;

    #if defined(STM32F0)
    uart_clk = HAL_RCC_GetPCLK1Freq();
    #elif defined(STM32F7)
    switch ((RCC->DCKCFGR2 >> ((self->uart_id - 1) * 2)) & 3) {
        case 0:
            if (self->uart_id == 1 || self->uart_id == 6) {
                uart_clk = HAL_RCC_GetPCLK2Freq();
            } else {
                uart_clk = HAL_RCC_GetPCLK1Freq();
            }
            break;
        case 1:
            uart_clk = HAL_RCC_GetSysClockFreq();
            break;
        case 2:
            uart_clk = HSI_VALUE;
            break;
        case 3:
            uart_clk = LSE_VALUE;
            break;
    }
    #elif defined(STM32H7)
    uint32_t csel;
    if (self->uart_id == 1 || self->uart_id == 6) {
        csel = RCC->D2CCIP2R >> 3;
    } else {
        csel = RCC->D2CCIP2R;
    }
    switch (csel & 3) {
        case 0:
            if (self->uart_id == 1 || self->uart_id == 6) {
                uart_clk = HAL_RCC_GetPCLK2Freq();
            } else {
                uart_clk = HAL_RCC_GetPCLK1Freq();
            }
            break;
        case 3:
            uart_clk = HSI_VALUE;
            break;
        case 4:
            uart_clk = CSI_VALUE;
            break;
        case 5:
            uart_clk = LSE_VALUE;
            break;
        default:
            break;
    }
    #else
    if (self->uart_id == 1
        #if defined(USART6)
        || self->uart_id == 6
        #endif
        #if defined(UART9)
        || self->uart_id == 9
        #endif
        #if defined(UART10)
        || self->uart_id == 10
        #endif
        ) {
        uart_clk = HAL_RCC_GetPCLK2Freq();
    } else {
        uart_clk = HAL_RCC_GetPCLK1Freq();
    }
    #endif

    // This formula assumes UART_OVERSAMPLING_16
    uint32_t baudrate = uart_clk / self->uartx->BRR;

    return baudrate;
}

mp_uint_t uart_rx_any(pyb_uart_obj_t *self) {
    int buffer_bytes = self->read_buf_head - self->read_buf_tail;
    if (buffer_bytes < 0) {
        return buffer_bytes + self->read_buf_len;
    } else if (buffer_bytes > 0) {
        return buffer_bytes;
    } else {
        return UART_RXNE_IS_SET(self->uartx) != 0;
    }
}

// Waits at most timeout milliseconds for at least 1 char to become ready for
// reading (from buf or for direct reading).
// Returns true if something available, false if not.
bool uart_rx_wait(pyb_uart_obj_t *self, uint32_t timeout) {
    uint32_t start = HAL_GetTick();
    for (;;) {
        if (self->read_buf_tail != self->read_buf_head || UART_RXNE_IS_SET(self->uartx)) {
            return true; // have at least 1 char ready for reading
        }
        if (HAL_GetTick() - start >= timeout) {
            return false; // timeout
        }
        MICROPY_EVENT_POLL_HOOK
    }
}

// assumes there is a character available
int uart_rx_char(pyb_uart_obj_t *self) {
    if (self->read_buf_tail != self->read_buf_head) {
        // buffering via IRQ
        int data;
        if (self->char_width == CHAR_WIDTH_9BIT) {
            data = ((uint16_t *)self->read_buf)[self->read_buf_tail];
        } else {
            data = self->read_buf[self->read_buf_tail];
        }
        self->read_buf_tail = (self->read_buf_tail + 1) % self->read_buf_len;
        if (UART_RXNE_IS_SET(self->uartx)) {
            // UART was stalled by flow ctrl: re-enable IRQ now we have room in buffer
            UART_RXNE_IT_EN(self->uartx);
        }
        return data;
    } else {
        // no buffering
        #if defined(STM32F0) || defined(STM32F7) || defined(STM32L0) || defined(STM32L4) || defined(STM32H7) || defined(STM32WB)
        int data = self->uartx->RDR & self->char_mask;
        self->uartx->ICR = USART_ICR_ORECF; // clear ORE if it was set
        return data;
        #else
        return self->uartx->DR & self->char_mask;
        #endif
    }
}

// Waits at most timeout milliseconds for TX register to become empty.
// Returns true if can write, false if can't.
bool uart_tx_wait(pyb_uart_obj_t *self, uint32_t timeout) {
    uint32_t start = HAL_GetTick();
    for (;;) {
        if (uart_tx_avail(self)) {
            return true; // tx register is empty
        }
        if (HAL_GetTick() - start >= timeout) {
            return false; // timeout
        }
        MICROPY_EVENT_POLL_HOOK
    }
}

// Waits at most timeout milliseconds for UART flag to be set.
// Returns true if flag is/was set, false on timeout.
STATIC bool uart_wait_flag_set(pyb_uart_obj_t *self, uint32_t flag, uint32_t timeout) {
    // Note: we don't use WFI to idle in this loop because UART tx doesn't generate
    // an interrupt and the flag can be set quickly if the baudrate is large.
    uint32_t start = HAL_GetTick();
    for (;;) {
        #if defined(STM32F4)
        if (self->uartx->SR & flag) {
            return true;
        }
        #else
        if (self->uartx->ISR & flag) {
            return true;
        }
        #endif
        if (timeout == 0 || HAL_GetTick() - start >= timeout) {
            return false; // timeout
        }
    }
}

// src - a pointer to the data to send (16-bit aligned for 9-bit chars)
// num_chars - number of characters to send (9-bit chars count for 2 bytes from src)
// *errcode - returns 0 for success, MP_Exxx on error
// returns the number of characters sent (valid even if there was an error)
size_t uart_tx_data(pyb_uart_obj_t *self, const void *src_in, size_t num_chars, int *errcode) {
    if (num_chars == 0) {
        *errcode = 0;
        return 0;
    }

    uint32_t timeout;
    if (self->uartx->CR3 & USART_CR3_CTSE) {
        // CTS can hold off transmission for an arbitrarily long time. Apply
        // the overall timeout rather than the character timeout.
        timeout = self->timeout;
    } else {
        // The timeout specified here is for waiting for the TX data register to
        // become empty (ie between chars), as well as for the final char to be
        // completely transferred.  The default value for timeout_char is long
        // enough for 1 char, but we need to double it to wait for the last char
        // to be transferred to the data register, and then to be transmitted.
        timeout = 2 * self->timeout_char;
    }

    const uint8_t *src = (const uint8_t *)src_in;
    size_t num_tx = 0;
    USART_TypeDef *uart = self->uartx;

    while (num_tx < num_chars) {
        if (!uart_wait_flag_set(self, UART_FLAG_TXE, timeout)) {
            *errcode = MP_ETIMEDOUT;
            return num_tx;
        }
        uint32_t data;
        if (self->char_width == CHAR_WIDTH_9BIT) {
            data = *((uint16_t *)src) & 0x1ff;
            src += 2;
        } else {
            data = *src++;
        }
        #if defined(STM32F4)
        uart->DR = data;
        #else
        uart->TDR = data;
        #endif
        ++num_tx;
    }

    // wait for the UART frame to complete
    if (!uart_wait_flag_set(self, UART_FLAG_TC, timeout)) {
        *errcode = MP_ETIMEDOUT;
        return num_tx;
    }

    *errcode = 0;
    return num_tx;
}

void uart_tx_strn(pyb_uart_obj_t *uart_obj, const char *str, uint len) {
    int errcode;
    uart_tx_data(uart_obj, str, len, &errcode);
}

// this IRQ handler is set up to handle RXNE interrupts only
void uart_irq_handler(mp_uint_t uart_id) {
    // get the uart object
    pyb_uart_obj_t *self = MP_STATE_PORT(pyb_uart_obj_all)[uart_id - 1];

    if (self == NULL) {
        // UART object has not been set, so we can't do anything, not
        // even disable the IRQ.  This should never happen.
        return;
    }

    if (UART_RXNE_IS_SET(self->uartx)) {
        if (self->read_buf_len != 0) {
            uint16_t next_head = (self->read_buf_head + 1) % self->read_buf_len;
            if (next_head != self->read_buf_tail) {
                // only read data if room in buf
                #if defined(STM32F0) || defined(STM32F7) || defined(STM32L0) || defined(STM32L4) || defined(STM32H7) || defined(STM32WB)
                int data = self->uartx->RDR; // clears UART_FLAG_RXNE
                self->uartx->ICR = USART_ICR_ORECF; // clear ORE if it was set
                #else
                int data = self->uartx->DR; // clears UART_FLAG_RXNE
                #endif
                data &= self->char_mask;
                if (self->attached_to_repl && data == mp_interrupt_char) {
                    // Handle interrupt coming in on a UART REPL
                    pendsv_kbd_intr();
                } else {
                    if (self->char_width == CHAR_WIDTH_9BIT) {
                        ((uint16_t *)self->read_buf)[self->read_buf_head] = data;
                    } else {
                        self->read_buf[self->read_buf_head] = data;
                    }
                    self->read_buf_head = next_head;
                }
            } else { // No room: leave char in buf, disable interrupt
                UART_RXNE_IT_DIS(self->uartx);
            }
        }
    }
    // If RXNE is clear but ORE set then clear the ORE flag (it's tied to RXNE IRQ)
    #if defined(STM32F4)
    else if (self->uartx->SR & USART_SR_ORE) {
        (void)self->uartx->DR;
    }
    #else
    else if (self->uartx->ISR & USART_ISR_ORE) {
        self->uartx->ICR = USART_ICR_ORECF;
    }
    #endif

    // Set user IRQ flags
    self->mp_irq_flags = 0;
    #if defined(STM32F4)
    if (self->uartx->SR & USART_SR_IDLE) {
        (void)self->uartx->SR;
        (void)self->uartx->DR;
        self->mp_irq_flags |= UART_FLAG_IDLE;
    }
    #else
    if (self->uartx->ISR & USART_ISR_IDLE) {
        self->uartx->ICR = USART_ICR_IDLECF;
        self->mp_irq_flags |= UART_FLAG_IDLE;
    }
    #endif

    // Check the flags to see if the user handler should be called
    if (self->mp_irq_trigger & self->mp_irq_flags) {
        mp_irq_handler(self->mp_irq_obj);
    }
}
