#include <stdint.h>
#include <stdio.h>

#include "stm32f4xx_hal.h"

#include "misc.h"
#include "mpconfig.h"
#include "qstr.h"
#include "obj.h"
#include "gc.h"
#include "gccollect.h"
#include "systick.h"
#include "pybstdio.h"
#include "pyexec.h"
#include "led.h"
#include "pin.h"
#include "timer.h"
#include "extint.h"
#include "usrsw.h"
#include "rng.h"
#include "rtc.h"
#include "i2c.h"
#include "spi.h"
#include "uart.h"
#include "adc.h"
#include "storage.h"
#include "sdcard.h"
#include "accel.h"
#include "servo.h"
#include "dac.h"
#include "usb.h"
#include "modpyb.h"
#include "ff.h"

// print lots of info about the board
STATIC mp_obj_t pyb_info(uint n_args, const mp_obj_t *args) {
    // get and print unique id; 96 bits
    {
        byte *id = (byte*)0x1fff7a10;
        printf("ID=%02x%02x%02x%02x:%02x%02x%02x%02x:%02x%02x%02x%02x\n", id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7], id[8], id[9], id[10], id[11]);
    }

    // get and print clock speeds
    // SYSCLK=168MHz, HCLK=168MHz, PCLK1=42MHz, PCLK2=84MHz
    {
        printf("S=%lu\nH=%lu\nP1=%lu\nP2=%lu\n", 
               HAL_RCC_GetSysClockFreq(),
               HAL_RCC_GetHCLKFreq(),
               HAL_RCC_GetPCLK1Freq(),
               HAL_RCC_GetPCLK2Freq());
    }

    // to print info about memory
    {
        printf("_etext=%p\n", &_etext);
        printf("_sidata=%p\n", &_sidata);
        printf("_sdata=%p\n", &_sdata);
        printf("_edata=%p\n", &_edata);
        printf("_sbss=%p\n", &_sbss);
        printf("_ebss=%p\n", &_ebss);
        printf("_estack=%p\n", &_estack);
        printf("_ram_start=%p\n", &_ram_start);
        printf("_heap_start=%p\n", &_heap_start);
        printf("_heap_end=%p\n", &_heap_end);
        printf("_ram_end=%p\n", &_ram_end);
    }

    // qstr info
    {
        uint n_pool, n_qstr, n_str_data_bytes, n_total_bytes;
        qstr_pool_info(&n_pool, &n_qstr, &n_str_data_bytes, &n_total_bytes);
        printf("qstr:\n  n_pool=%u\n  n_qstr=%u\n  n_str_data_bytes=%u\n  n_total_bytes=%u\n", n_pool, n_qstr, n_str_data_bytes, n_total_bytes);
    }

    // GC info
    {
        gc_info_t info;
        gc_info(&info);
        printf("GC:\n");
        printf("  %lu total\n", info.total);
        printf("  %lu : %lu\n", info.used, info.free);
        printf("  1=%lu 2=%lu m=%lu\n", info.num_1block, info.num_2block, info.max_block);
    }

    // free space on flash
    {
        DWORD nclst;
        FATFS *fatfs;
        f_getfree("0:", &nclst, &fatfs);
        printf("LFS free: %u bytes\n", (uint)(nclst * fatfs->csize * 512));
    }

    if (n_args == 1) {
        // arg given means dump gc allocation table
        gc_dump_alloc_table();
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(pyb_info_obj, 0, 1, pyb_info);

// get unique MCU id; 96 bits = 12 bytes
STATIC mp_obj_t pyb_unique_id(void) {
    byte *id = (byte*)0x1fff7a10;
    return mp_obj_new_bytes(id, 12);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(pyb_unique_id_obj, pyb_unique_id);

// get clock frequencies
// TODO should also be able to set frequency via this function
STATIC mp_obj_t pyb_freq(void) {
    mp_obj_t tuple[4] = {
       mp_obj_new_int(HAL_RCC_GetSysClockFreq()),
       mp_obj_new_int(HAL_RCC_GetHCLKFreq()),
       mp_obj_new_int(HAL_RCC_GetPCLK1Freq()),
       mp_obj_new_int(HAL_RCC_GetPCLK2Freq()),
    };
    return mp_obj_new_tuple(4, tuple);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(pyb_freq_obj, pyb_freq);

// sync all file systems
STATIC mp_obj_t pyb_sync(void) {
    storage_flush();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(pyb_sync_obj, pyb_sync);

STATIC mp_obj_t pyb_millis(void) {
    return mp_obj_new_int(HAL_GetTick());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(pyb_millis_obj, pyb_millis);

STATIC mp_obj_t pyb_delay(mp_obj_t count) {
    HAL_Delay(mp_obj_get_int(count));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(pyb_delay_obj, pyb_delay);

STATIC mp_obj_t pyb_udelay(mp_obj_t usec) {
    uint32_t count = 0;
    const uint32_t utime = (168 * mp_obj_get_int(usec) / 5);
    for (;;) {
        if (++count > utime) {
            return mp_const_none;
        }
    }
}

STATIC MP_DEFINE_CONST_FUN_OBJ_1(pyb_udelay_obj, pyb_udelay);

STATIC mp_obj_t pyb_wfi(void) {
    __WFI();
    return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_0(pyb_wfi_obj, pyb_wfi);

STATIC mp_obj_t pyb_disable_irq(void) {
    __disable_irq();
    return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_0(pyb_disable_irq_obj, pyb_disable_irq);

STATIC mp_obj_t pyb_enable_irq(void) {
    __enable_irq();
    return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_0(pyb_enable_irq_obj, pyb_enable_irq);

#if 0
STATIC void SYSCLKConfig_STOP(void) {
    /* After wake-up from STOP reconfigure the system clock */
    /* Enable HSE */
    RCC_HSEConfig(RCC_HSE_ON);

    /* Wait till HSE is ready */
    while (RCC_GetFlagStatus(RCC_FLAG_HSERDY) == RESET) {
    }

    /* Enable PLL */
    RCC_PLLCmd(ENABLE);

    /* Wait till PLL is ready */
    while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET) {
    }

    /* Select PLL as system clock source */
    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

    /* Wait till PLL is used as system clock source */
    while (RCC_GetSYSCLKSource() != 0x08) {
    }
}
#endif

STATIC mp_obj_t pyb_stop(void) {
#if 0
    PWR_EnterSTANDBYMode();
    //PWR_FlashPowerDownCmd(ENABLE); don't know what the logic is with this

    /* Enter Stop Mode */
    PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);

    /* Configures system clock after wake-up from STOP: enable HSE, PLL and select 
     *        PLL as system clock source (HSE and PLL are disabled in STOP mode) */
    SYSCLKConfig_STOP();

    //PWR_FlashPowerDownCmd(DISABLE);
#endif
    return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_0(pyb_stop_obj, pyb_stop);

STATIC mp_obj_t pyb_standby(void) {
#if 0
    PWR_EnterSTANDBYMode();
#endif
    return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_0(pyb_standby_obj, pyb_standby);

STATIC mp_obj_t pyb_have_cdc(void ) {
    return MP_BOOL(usb_vcp_is_connected());
}

STATIC MP_DEFINE_CONST_FUN_OBJ_0(pyb_have_cdc_obj, pyb_have_cdc);

STATIC mp_obj_t pyb_hid_send_report(mp_obj_t arg) {
    mp_obj_t *items;
    mp_obj_get_array_fixed_n(arg, 4, &items);
    uint8_t data[4];
    data[0] = mp_obj_get_int(items[0]);
    data[1] = mp_obj_get_int(items[1]);
    data[2] = mp_obj_get_int(items[2]);
    data[3] = mp_obj_get_int(items[3]);
    usb_hid_send_report(data);
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_1(pyb_hid_send_report_obj, pyb_hid_send_report);

MP_DECLARE_CONST_FUN_OBJ(pyb_source_dir_obj); // defined in main.c
MP_DECLARE_CONST_FUN_OBJ(pyb_main_obj); // defined in main.c
MP_DECLARE_CONST_FUN_OBJ(pyb_usb_mode_obj); // defined in main.c

STATIC const mp_map_elem_t pyb_module_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_pyb) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_info), (mp_obj_t)&pyb_info_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_unique_id), (mp_obj_t)&pyb_unique_id_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_freq), (mp_obj_t)&pyb_freq_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_gc), (mp_obj_t)&pyb_gc_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_repl_info), (mp_obj_t)&pyb_set_repl_info_obj },

    { MP_OBJ_NEW_QSTR(MP_QSTR_wfi), (mp_obj_t)&pyb_wfi_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_disable_irq), (mp_obj_t)&pyb_disable_irq_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_enable_irq), (mp_obj_t)&pyb_enable_irq_obj },

    { MP_OBJ_NEW_QSTR(MP_QSTR_stop), (mp_obj_t)&pyb_stop_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_standby), (mp_obj_t)&pyb_standby_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_source_dir), (mp_obj_t)&pyb_source_dir_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_main), (mp_obj_t)&pyb_main_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_usb_mode), (mp_obj_t)&pyb_usb_mode_obj },

    { MP_OBJ_NEW_QSTR(MP_QSTR_have_cdc), (mp_obj_t)&pyb_have_cdc_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_hid), (mp_obj_t)&pyb_hid_send_report_obj },

    { MP_OBJ_NEW_QSTR(MP_QSTR_millis), (mp_obj_t)&pyb_millis_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_delay), (mp_obj_t)&pyb_delay_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_udelay), (mp_obj_t)&pyb_udelay_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sync), (mp_obj_t)&pyb_sync_obj },

    { MP_OBJ_NEW_QSTR(MP_QSTR_Timer), (mp_obj_t)&pyb_timer_type },

#if MICROPY_HW_ENABLE_RNG
    { MP_OBJ_NEW_QSTR(MP_QSTR_rng), (mp_obj_t)&pyb_rng_get_obj },
#endif

#if MICROPY_HW_ENABLE_RTC
    { MP_OBJ_NEW_QSTR(MP_QSTR_RTC), (mp_obj_t)&pyb_rtc_type },
#endif

    { MP_OBJ_NEW_QSTR(MP_QSTR_Pin), (mp_obj_t)&pin_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ExtInt), (mp_obj_t)&extint_type },

#if MICROPY_HW_ENABLE_SERVO
    { MP_OBJ_NEW_QSTR(MP_QSTR_pwm), (mp_obj_t)&pyb_pwm_set_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_servo), (mp_obj_t)&pyb_servo_set_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_Servo), (mp_obj_t)&pyb_servo_type },
#endif

#if MICROPY_HW_HAS_SWITCH
    { MP_OBJ_NEW_QSTR(MP_QSTR_Switch), (mp_obj_t)&pyb_switch_type },
#endif

#if MICROPY_HW_HAS_SDCARD
    { MP_OBJ_NEW_QSTR(MP_QSTR_SD), (mp_obj_t)&pyb_sdcard_obj },
#endif

    { MP_OBJ_NEW_QSTR(MP_QSTR_LED), (mp_obj_t)&pyb_led_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_I2C), (mp_obj_t)&pyb_i2c_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SPI), (mp_obj_t)&pyb_spi_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_UART), (mp_obj_t)&pyb_uart_type },

    { MP_OBJ_NEW_QSTR(MP_QSTR_ADC), (mp_obj_t)&pyb_adc_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADCAll), (mp_obj_t)&pyb_adc_all_type },

#if MICROPY_HW_ENABLE_DAC
    { MP_OBJ_NEW_QSTR(MP_QSTR_DAC), (mp_obj_t)&pyb_dac_type },
#endif

#if MICROPY_HW_HAS_MMA7660
    { MP_OBJ_NEW_QSTR(MP_QSTR_Accel), (mp_obj_t)&pyb_accel_type },
#endif
};

STATIC const mp_obj_dict_t pyb_module_globals = {
    .base = {&mp_type_dict},
    .map = {
        .all_keys_are_qstrs = 1,
        .table_is_fixed_array = 1,
        .used = sizeof(pyb_module_globals_table) / sizeof(mp_map_elem_t),
        .alloc = sizeof(pyb_module_globals_table) / sizeof(mp_map_elem_t),
        .table = (mp_map_elem_t*)pyb_module_globals_table,
    },
};

const mp_obj_module_t pyb_module = {
    .base = { &mp_type_module },
    .name = MP_QSTR_pyb,
    .globals = (mp_obj_dict_t*)&pyb_module_globals,
};
