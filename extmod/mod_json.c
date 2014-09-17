/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Damien P. George
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
#include <unistd.h>
#include <string.h>

#include "mpconfig.h"
#include "misc.h"
#include "qstr.h"
#include "obj.h"
#include "runtime.h"

#if MICROPY_PY__JSON

STATIC mp_obj_t mod__json_dumps(mp_obj_t obj) {
    vstr_t vstr;
    vstr_init(&vstr, 8);
    mp_obj_print_helper((void (*)(void *env, const char *fmt, ...))vstr_printf, &vstr, obj, PRINT_JSON);
    mp_obj_t ret = mp_obj_new_str(vstr.buf, vstr.len, false);
    vstr_clear(&vstr);
    return ret;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod__json_dumps_obj, mod__json_dumps);

STATIC const mp_map_elem_t mp_module__json_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR__json) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_dumps), (mp_obj_t)&mod__json_dumps_obj },
};

STATIC const mp_obj_dict_t mp_module__json_globals = {
    .base = {&mp_type_dict},
    .map = {
        .all_keys_are_qstrs = 1,
        .table_is_fixed_array = 1,
        .used = MP_ARRAY_SIZE(mp_module__json_globals_table),
        .alloc = MP_ARRAY_SIZE(mp_module__json_globals_table),
        .table = (mp_map_elem_t*)mp_module__json_globals_table,
    },
};

const mp_obj_module_t mp_module__json = {
    .base = { &mp_type_module },
    .name = MP_QSTR__json,
    .globals = (mp_obj_dict_t*)&mp_module__json_globals,
};

#endif //MICROPY_PY__JSON
