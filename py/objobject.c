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

#include <stdlib.h>

#include "mpconfig.h"
#include "nlr.h"
#include "misc.h"
#include "qstr.h"
#include "obj.h"
#include "runtime0.h"

typedef struct _mp_obj_object_t {
    mp_obj_base_t base;
} mp_obj_object_t;

STATIC mp_obj_t object_make_new(mp_obj_t type_in, uint n_args, uint n_kw, const mp_obj_t *args) {
    if (n_args != 0 || n_kw != 0) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_TypeError, "object takes no arguments"));
    }

    mp_obj_object_t *o = m_new_obj(mp_obj_object_t);
    o->base.type = &mp_type_object;
    return o;
}

const mp_obj_type_t mp_type_object = {
    { &mp_type_type },
    .name = MP_QSTR_object,
    .make_new = object_make_new,
};
