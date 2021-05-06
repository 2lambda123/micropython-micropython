/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Philipp Ebensberger
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
#include <stdint.h>

#include "fsl_gpio.h"
#include "fsl_iomuxc.h"

#include "py/runtime.h"
#include "py/mphal.h"
#include "pin.h"
#include "mphalport.h"

// Local functions
STATIC mp_obj_t pin_obj_init_helper(machine_pin_obj_t *self, size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);
STATIC void named_pin_obj_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind);

// Class Methods
STATIC void machine_pin_obj_print(const mp_print_t *print, mp_obj_t o, mp_print_kind_t kind);
STATIC mp_obj_t machine_pin_call(mp_obj_t self_in, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args);
STATIC mp_obj_t machine_pin_obj_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args);

// Instance Methods
STATIC mp_obj_t machine_pin_off(mp_obj_t self_in);
STATIC mp_obj_t machine_pin_on(mp_obj_t self_in);
STATIC mp_obj_t machine_pin_value(size_t n_args, const mp_obj_t *args);
STATIC mp_obj_t machine_pin_mode(mp_obj_t self_in);

// Local data
enum {
    PIN_INIT_ARG_MODE = 0,
    PIN_INIT_ARG_VALUE
};

// Pin mapping dictionaries
const mp_obj_type_t machine_pin_cpu_pins_obj_type = {
    { &mp_type_type },
    .name = MP_QSTR_cpu,
    .print = named_pin_obj_print,
    .locals_dict = (mp_obj_t)&machine_pin_cpu_pins_locals_dict,
};

const mp_obj_type_t machine_pin_board_pins_obj_type = {
    { &mp_type_type },
    .name = MP_QSTR_board,
    .print = named_pin_obj_print,
    .locals_dict = (mp_obj_t)&machine_pin_board_pins_locals_dict,
};


STATIC mp_obj_t machine_pin_call(mp_obj_t self_in, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 1, false);
    machine_pin_obj_t *self = self_in;

    if (n_args == 0) {
        return MP_OBJ_NEW_SMALL_INT(mp_hal_pin_read(self));
    } else {
        if (MP_OBJ_SMALL_INT_VALUE(args[0]) == 1U) {
            mp_hal_pin_high(self);
        } else {
            mp_hal_pin_low(self);
        }
        return mp_const_none;
    }
}

STATIC mp_obj_t pin_obj_init_helper(machine_pin_obj_t *self, size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        [PIN_INIT_ARG_MODE] { MP_QSTR_mode, MP_ARG_REQUIRED | MP_ARG_INT },
        [PIN_INIT_ARG_VALUE] { MP_QSTR_value, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        // TODO: Implement additional arguments
        /*{ MP_QSTR_pull, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE}},
        { MP_QSTR_af, MP_ARG_INT, {.u_int = -1}}, // legacy
        { MP_QSTR_alt, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1}},*/
    };

    // Parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // Get io mode
    uint mode = args[PIN_INIT_ARG_MODE].u_int;
    if (!IS_GPIO_MODE(mode)) {
        mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("invalid pin mode: %d"), mode);
    }

    // Handle configuration for GPIO
    if ((mode == PIN_MODE_IN) || (mode == PIN_MODE_OUT)) {
        gpio_pin_config_t pin_config;
        const machine_pin_af_obj_t *af_obj;
        uint32_t pad_config = 0UL;

        if ((args[PIN_INIT_ARG_VALUE].u_obj != MP_OBJ_NULL) && (mp_obj_is_true(args[PIN_INIT_ARG_VALUE].u_obj))) {
            pin_config.outputLogic = 1U;
        } else {
            pin_config.outputLogic = 0U;
        }
        pin_config.direction = mode == PIN_MODE_IN ? kGPIO_DigitalInput : kGPIO_DigitalOutput;
        pin_config.interruptMode = kGPIO_NoIntmode;

        af_obj = pin_find_af(self, PIN_AF_MODE_ALT5);  // GPIO is always ALT5
        if (af_obj == NULL) {
            mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("requested AF %d not available for pin %d"), PIN_AF_MODE_ALT5, mode);
        }

        // Update machine pin object
        self->mode = mode;

        // Update pad configuration
        if (mode == PIN_MODE_IN) {
            pad_config = IOMUXC_SW_PAD_CTL_PAD_SRE(0U) |
                IOMUXC_SW_PAD_CTL_PAD_DSE(0b000) |  // output driver disabled
                IOMUXC_SW_PAD_CTL_PAD_SPEED(0b01) |  // medium(100MHz)
                IOMUXC_SW_PAD_CTL_PAD_ODE(0b0) |  // Open Drain Disabled
                IOMUXC_SW_PAD_CTL_PAD_PKE(0b1) |  // Pull/Keeper Enabled
                IOMUXC_SW_PAD_CTL_PAD_PUE(0b1) |  // Pull selected
                IOMUXC_SW_PAD_CTL_PAD_PUS(0b00) |  // 100K Ohm Pull Down
                IOMUXC_SW_PAD_CTL_PAD_HYS(1U);  // Hysteresis enabled
        } else {
            pad_config = af_obj->pad_config;
        }

        // Configure pad as GPIO
        IOMUXC_SetPinMux(self->muxRegister, af_obj->af_mode, 0, 0, self->configRegister, 1U);  // Software Input On Field: Input Path is determined by functionality
        IOMUXC_SetPinConfig(self->muxRegister, af_obj->af_mode, 0, 0, self->configRegister, pad_config);
        GPIO_PinInit(self->gpio, self->pin, &pin_config);
    }

    return mp_const_none;
}

STATIC void named_pin_obj_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    machine_pin_obj_t *self = self_in;
    mp_printf(print, "<Pin.%q>", self->name);
}

STATIC void machine_pin_obj_print(const mp_print_t *print, mp_obj_t o, mp_print_kind_t kind) {
    (void)kind;
    machine_pin_obj_t *self = MP_OBJ_TO_PTR(o);
    mp_printf(print, "PIN(%s)", qstr_str(self->name));
}

// constructor(id, ...)
STATIC mp_obj_t machine_pin_obj_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, MP_OBJ_FUN_ARGS_MAX, true);

    machine_pin_obj_t *pin = pin_find(args[0]);

    if (n_args > 1 || n_kw > 0) {
        // pin mode given, so configure this GPIO
        mp_map_t kw_args;
        mp_map_init_fixed_table(&kw_args, n_kw, args + n_args);
        pin_obj_init_helper(pin, n_args - 1, args + 1, &kw_args);
    }

    return (mp_obj_t)pin;
}

// pin.off()
STATIC mp_obj_t machine_pin_off(mp_obj_t self_in) {
    machine_pin_obj_t *self = self_in;
    mp_hal_pin_low(self);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(machine_pin_off_obj, machine_pin_off);

// pin.on()
STATIC mp_obj_t machine_pin_on(mp_obj_t self_in) {
    machine_pin_obj_t *self = self_in;
    mp_hal_pin_high(self);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(machine_pin_on_obj, machine_pin_on);

// pin.value()
STATIC mp_obj_t machine_pin_value(size_t n_args, const mp_obj_t *args) {
    machine_pin_obj_t *self = args[0];
    if (n_args > 1) {
        if (MP_OBJ_SMALL_INT_VALUE(args[1]) == 1U) {
            mp_hal_pin_high(self);
        } else {
            mp_hal_pin_low(self);
        }
        return mp_const_none;
    } else {
        return MP_OBJ_NEW_SMALL_INT(mp_hal_pin_read(self));
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_pin_value_obj, 1, 2, machine_pin_value);

STATIC mp_obj_t machine_pin_mode(mp_obj_t self_in) {
    machine_pin_obj_t *self = self_in;
    return mp_obj_new_int(self->mode);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(machine_pin_mode_obj, machine_pin_mode);


STATIC const mp_rom_map_elem_t machine_pin_locals_dict_table[] = {
    // TODO: Implement class locals dictionary
    // instance methods
    { MP_ROM_QSTR(MP_QSTR_off),     MP_ROM_PTR(&machine_pin_off_obj) },
    { MP_ROM_QSTR(MP_QSTR_on),      MP_ROM_PTR(&machine_pin_on_obj) },
    { MP_ROM_QSTR(MP_QSTR_low),     MP_ROM_PTR(&machine_pin_off_obj) },
    { MP_ROM_QSTR(MP_QSTR_high),    MP_ROM_PTR(&machine_pin_on_obj) },
    { MP_ROM_QSTR(MP_QSTR_value),   MP_ROM_PTR(&machine_pin_value_obj) },
    { MP_ROM_QSTR(MP_QSTR_mode),    MP_ROM_PTR(&machine_pin_mode_obj) },
    // class attributes
    { MP_ROM_QSTR(MP_QSTR_board),   MP_ROM_PTR(&machine_pin_board_pins_obj_type) },
    { MP_ROM_QSTR(MP_QSTR_cpu),     MP_ROM_PTR(&machine_pin_cpu_pins_obj_type) },
    // class constants
    { MP_ROM_QSTR(MP_QSTR_IN),      MP_ROM_INT(PIN_MODE_IN) },
    { MP_ROM_QSTR(MP_QSTR_OUT),     MP_ROM_INT(PIN_MODE_OUT) },
};
STATIC MP_DEFINE_CONST_DICT(machine_pin_locals_dict, machine_pin_locals_dict_table);


const mp_obj_type_t machine_pin_type = {
    {&mp_type_type},
    .name = MP_QSTR_Pin,
    .print = machine_pin_obj_print,
    .call = machine_pin_call,
    .make_new = machine_pin_obj_make_new,
    .locals_dict = (mp_obj_dict_t *)&machine_pin_locals_dict,
};

// FIXME: Create actual pin_af type!!!
const mp_obj_type_t machine_pin_af_type = {
    {&mp_type_type},
    .name = MP_QSTR_PinAF,
    .print = machine_pin_obj_print,
    .make_new = machine_pin_obj_make_new,
    .locals_dict = (mp_obj_dict_t *)&machine_pin_locals_dict,
};
