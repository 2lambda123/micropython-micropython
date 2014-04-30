/*
 * This file is part of the Micro Python project, http://micropython.org/
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

#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>

#include "misc.h"
#include "mpconfig.h"
#include "qstr.h"
#include "obj.h"
#include "runtime.h"

STATIC mp_obj_t mod_time_time() {
#if MICROPY_ENABLE_FLOAT
    struct timeval tv;
    gettimeofday(&tv, NULL);
    mp_float_t val = tv.tv_sec + (mp_float_t)tv.tv_usec / 1000000;
    return mp_obj_new_float(val);
#else
    return mp_obj_new_int((machine_int_t)time(NULL));
#endif
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_time_time_obj, mod_time_time);

// Note: this is deprecated since CPy3.3, but pystone still uses it.
STATIC mp_obj_t mod_time_clock() {
#if MICROPY_ENABLE_FLOAT
    // POSIX requires CLOCKS_PER_SEC equals 1000000, so that's what we assume.
    // float cannot represent full range of int32 precisely, so we pre-divide
    // int to reduce resolution, and then actually do float division hoping
    // to preserve integer part resolution.
    return mp_obj_new_float((float)(clock() / 1000) / 1000.0);
#else
    return mp_obj_new_int((machine_int_t)clock());
#endif
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_time_clock_obj, mod_time_clock);

STATIC mp_obj_t mod_time_sleep(mp_obj_t arg) {
#if MICROPY_ENABLE_FLOAT
    struct timeval tv;
    mp_float_t val = mp_obj_get_float(arg);
    double ipart;
    tv.tv_usec = round(modf(val, &ipart) * 1000000);
    tv.tv_sec = ipart;
    select(0, NULL, NULL, NULL, &tv);
#else
    sleep(mp_obj_get_int(arg));
#endif
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_time_sleep_obj, mod_time_sleep);

STATIC const mp_map_elem_t mp_module_time_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_time) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_clock), (mp_obj_t)&mod_time_clock_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sleep), (mp_obj_t)&mod_time_sleep_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_time), (mp_obj_t)&mod_time_time_obj },
};

STATIC const mp_obj_dict_t mp_module_time_globals = {
    .base = {&mp_type_dict},
    .map = {
        .all_keys_are_qstrs = 1,
        .table_is_fixed_array = 1,
        .used = ARRAY_SIZE(mp_module_time_globals_table),
        .alloc = ARRAY_SIZE(mp_module_time_globals_table),
        .table = (mp_map_elem_t*)mp_module_time_globals_table,
    },
};

const mp_obj_module_t mp_module_time = {
    .base = { &mp_type_module },
    .name = MP_QSTR_time,
    .globals = (mp_obj_dict_t*)&mp_module_time_globals,
};
