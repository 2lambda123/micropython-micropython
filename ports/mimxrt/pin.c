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

#include "py/runtime.h"
#include "pin.h"


// Declaration in generated header file pins.h
extern const machine_pin_obj_t machine_board_pin_obj[];
extern const uint32_t machine_pin_num_of_board_pins;


void pin_init(void) {
    return;
}

uint32_t pin_get_mode(const machine_pin_obj_t *pin) {
    return pin->mode;
}

uint32_t pin_get_pull(const machine_pin_obj_t *pin) {
    // TODO: implement get_pull function
    return 0U;
}

uint32_t pin_get_af(const machine_pin_obj_t *pin) {
    return pin->af_mode;
}

const machine_pin_obj_t *pin_find(mp_obj_t user_obj) {
    const machine_pin_obj_t *pin_obj;

    // If pin is SMALL_INT
    if (mp_obj_is_small_int(user_obj)) {
        uint8_t value = MP_OBJ_SMALL_INT_VALUE(user_obj);

        for (uint8_t i = 0; i < machine_pin_num_of_board_pins; i++) {
            if (machine_board_pin_obj[i].pin == value) {
                return &machine_board_pin_obj[i];
            }
        }
    }

    // If a pin was provided, then use it
    if (mp_obj_is_type(user_obj, &machine_pin_type)) {
        pin_obj = user_obj;
        return pin_obj;
    }


    #if 0
    // TODO: Implement support for wrapper classes and dictionaries
    // If a pin mapper was provided
    if (MP_STATE_PORT(pin_class_mapper) != mp_const_none) {
        pin_obj = mp_call_function_1(MP_STATE_PORT(pin_class_mapper), user_obj);

        if (pin_obj != mp_const_none) {
            if (!mp_obj_is_type(pin_obj, &machine_pin_type)) {
                mp_raise_ValueError(MP_ERROR_TEXT("Pin.mapper didn't return a Pin object"));
            }
            return pin_obj;
        }
        // The pin mapping function returned mp_const_none, fall through to
        // other lookup methods.
    }

    // If a pin mapper dictionary was provided
    if (MP_STATE_PORT(pin_class_map_dict) != mp_const_none) {
        mp_map_t *pin_map_map = mp_obj_dict_get_map(MP_STATE_PORT(pin_class_map_dict));
        mp_map_elem_t *elem = mp_map_lookup(pin_map_map, user_obj, MP_MAP_LOOKUP);

        if (elem != NULL && elem->value != NULL) {
            pin_obj = elem->value;
            return pin_obj;
        }
    }
    #endif

    // See if the pin name matches a board pin
    pin_obj = pin_find_named_pin(&pin_board_pins_locals_dict, user_obj);
    if (pin_obj) {
        return pin_obj;
    }

    // See if the pin name matches a cpu pin
    pin_obj = pin_find_named_pin(&pin_cpu_pins_locals_dict, user_obj);
    if (pin_obj) {
        return pin_obj;
    }

    mp_raise_ValueError(MP_ERROR_TEXT("not a valid pin identifier"));
}

const machine_pin_obj_t *pin_find_named_pin(const mp_obj_dict_t *named_pins, mp_obj_t name) {
    mp_map_t *named_map = mp_obj_dict_get_map((mp_obj_t)named_pins);
    mp_map_elem_t *named_elem = mp_map_lookup(named_map, name, MP_MAP_LOOKUP);
    if (named_elem != NULL && named_elem->value != NULL) {
        return named_elem->value;
    }
    return NULL;
}

const machine_pin_af_obj_t *pin_find_af(const machine_pin_obj_t *pin, uint8_t fn, uint8_t unit) {
    // TODO: Implement pin_find_af function
    return NULL;
}

const machine_pin_af_obj_t *pin_find_af_by_index(const machine_pin_obj_t *pin, mp_uint_t af_idx) {
    // TODO: Implement pin_find_af_by_index function
    return NULL;
}

const machine_pin_af_obj_t *pin_find_af_by_name(const machine_pin_obj_t *pin, const char *name) {
    // TODO: Implement pin_find_af_by_name function
    return NULL;
}
