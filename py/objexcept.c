#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include "nlr.h"
#include "misc.h"
#include "mpconfig.h"
#include "obj.h"
#include "objtuple.h"

// This is unified class for C-level and Python-level exceptions
// Python-level exception have empty ->msg and all arguments are in
// args tuple. C-level excepttion likely have ->msg, and may as well
// have args tuple (or otherwise have it as NULL).
typedef struct mp_obj_exception_t {
    mp_obj_base_t base;
    qstr id;
    qstr msg;
    mp_obj_tuple_t args;
} mp_obj_exception_t;

void exception_print(void (*print)(void *env, const char *fmt, ...), void *env, mp_obj_t o_in, mp_print_kind_t kind) {
    mp_obj_exception_t *o = o_in;
    if (o->msg != 0) {
        print(env, "%s: %s", qstr_str(o->id), qstr_str(o->msg));
    } else {
        // Yes, that's how CPython has it
        if (kind == PRINT_REPR) {
            print(env, "%s", qstr_str(o->id));
        }
        if (kind == PRINT_STR) {
            if (o->args.len == 0) {
                print(env, "");
                return;
            } else if (o->args.len == 1) {
                mp_obj_print_helper(print, env, o->args.items[0], PRINT_STR);
                return;
            }
        }
        tuple_print(print, env, &o->args, kind);
    }
}

// args in reversed order
static mp_obj_t exception_call(mp_obj_t self_in, int n_args, const mp_obj_t *args) {
    mp_obj_exception_t *base = self_in;
    mp_obj_exception_t *o = m_new_obj_var(mp_obj_exception_t, mp_obj_t*, n_args);
    o->base.type = &exception_type;
    o->id = base->id;
    o->msg = 0;
    o->args.len = n_args;

    // TODO: factor out as reusable copy_reversed()
    int j = 0;
    for (int i = n_args - 1; i >= 0; i--) {
        o->args.items[i] = args[j++];
    }
    return o;
}

const mp_obj_type_t exception_type = {
    { &mp_const_type },
    "exception",
    .print = exception_print,
    .call_n = exception_call,
};

mp_obj_t mp_obj_new_exception(qstr id) {
    return mp_obj_new_exception_msg_varg(id, NULL);
}

mp_obj_t mp_obj_new_exception_msg(qstr id, const char *msg) {
    return mp_obj_new_exception_msg_varg(id, msg);
}

mp_obj_t mp_obj_new_exception_msg_1_arg(qstr id, const char *fmt, const char *a1) {
    return mp_obj_new_exception_msg_varg(id, fmt, a1);
}

mp_obj_t mp_obj_new_exception_msg_2_args(qstr id, const char *fmt, const char *a1, const char *a2) {
    return mp_obj_new_exception_msg_varg(id, fmt, a1, a2);
}

mp_obj_t mp_obj_new_exception_msg_varg(qstr id, const char *fmt, ...) {
    // make exception object
    mp_obj_exception_t *o = m_new_obj_var(mp_obj_exception_t, mp_obj_t*, 0);
    o->base.type = &exception_type;
    o->id = id;
    o->args.len = 0;
    if (fmt == NULL) {
        o->msg = 0;
    } else {
        // render exception message
        vstr_t *vstr = vstr_new();
        va_list ap;
        va_start(ap, fmt);
        vstr_vprintf(vstr, fmt, ap);
        va_end(ap);
        o->msg = qstr_from_str_take(vstr->buf, vstr->alloc);
    }

    return o;
}

qstr mp_obj_exception_get_type(mp_obj_t self_in) {
    assert(MP_OBJ_IS_TYPE(self_in, &exception_type));
    mp_obj_exception_t *self = self_in;
    return self->id;
}
