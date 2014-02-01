#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "nlr.h"
#include "misc.h"
#include "mpconfig.h"
#include "qstr.h"
#include "obj.h"
#include "objtuple.h"
#include "map.h"
#include "runtime.h"
#include "bc.h"

/******************************************************************************/
/* native functions                                                           */

// mp_obj_fun_native_t defined in obj.h

void check_nargs(mp_obj_fun_native_t *self, int n_args, int n_kw) {
    if (n_kw && !self->is_kw) {
        nlr_jump(mp_obj_new_exception_msg(MP_QSTR_TypeError,
                                          "function does not take keyword arguments"));
    }

    if (self->n_args_min == self->n_args_max) {
        if (n_args != self->n_args_min) {
            nlr_jump(mp_obj_new_exception_msg_2_args(MP_QSTR_TypeError,
                                                     "function takes %d positional arguments but %d were given",
                                                     (const char*)(machine_int_t)self->n_args_min,
                                                     (const char*)(machine_int_t)n_args));
        }
    } else {
        if (n_args < self->n_args_min) {
            nlr_jump(mp_obj_new_exception_msg_1_arg(MP_QSTR_TypeError,
                                                    "<fun name>() missing %d required positional arguments: <list of names of params>",
                                                    (const char*)(machine_int_t)(self->n_args_min - n_args)));
        } else if (n_args > self->n_args_max) {
            nlr_jump(mp_obj_new_exception_msg_2_args(MP_QSTR_TypeError,
                                                     "<fun name> expected at most %d arguments, got %d",
                                                     (void*)(machine_int_t)self->n_args_max, (void*)(machine_int_t)n_args));
        }
    }
}

mp_obj_t fun_native_call(mp_obj_t self_in, uint n_args, uint n_kw, const mp_obj_t *args) {
    assert(MP_OBJ_IS_TYPE(self_in, &fun_native_type));
    mp_obj_fun_native_t *self = self_in;

    // check number of arguments
    check_nargs(self, n_args, n_kw);

    if (self->is_kw) {
        // function allows keywords

        // TODO if n_kw==0 then don't allocate any memory for map (either pass NULL or allocate it on the heap)
        mp_map_t *kw_args = mp_map_new(n_kw);
        for (int i = 0; i < 2 * n_kw; i += 2) {
            mp_map_lookup(kw_args, args[n_args + i], MP_MAP_LOOKUP_ADD_IF_NOT_FOUND)->value = args[n_args + i + 1];
        }
        mp_obj_t res = ((mp_fun_kw_t)self->fun)(n_args, args, kw_args);
        // TODO clean up kw_args

        return res;

    } else if (self->n_args_min <= 3 && self->n_args_min == self->n_args_max) {
        // function requires a fixed number of arguments

        // dispatch function call
        switch (self->n_args_min) {
            case 0:
                return ((mp_fun_0_t)self->fun)();

            case 1:
                return ((mp_fun_1_t)self->fun)(args[0]);

            case 2:
                return ((mp_fun_2_t)self->fun)(args[0], args[1]);

            case 3:
                return ((mp_fun_3_t)self->fun)(args[0], args[1], args[2]);

            default:
                assert(0);
                return mp_const_none;
        }

    } else {
        // function takes a variable number of arguments, but no keywords

        return ((mp_fun_var_t)self->fun)(n_args, args);
    }
}

const mp_obj_type_t fun_native_type = {
    { &mp_const_type },
    "function",
    .call = fun_native_call,
};

// fun must have the correct signature for n_args fixed arguments
mp_obj_t rt_make_function_n(int n_args, void *fun) {
    mp_obj_fun_native_t *o = m_new_obj(mp_obj_fun_native_t);
    o->base.type = &fun_native_type;
    o->is_kw = false;
    o->n_args_min = n_args;
    o->n_args_max = n_args;
    o->fun = fun;
    return o;
}

mp_obj_t rt_make_function_var(int n_args_min, mp_fun_var_t fun) {
    mp_obj_fun_native_t *o = m_new_obj(mp_obj_fun_native_t);
    o->base.type = &fun_native_type;
    o->is_kw = false;
    o->n_args_min = n_args_min;
    o->n_args_max = ~((machine_uint_t)0);
    o->fun = fun;
    return o;
}

// min and max are inclusive
mp_obj_t rt_make_function_var_between(int n_args_min, int n_args_max, mp_fun_var_t fun) {
    mp_obj_fun_native_t *o = m_new_obj(mp_obj_fun_native_t);
    o->base.type = &fun_native_type;
    o->is_kw = false;
    o->n_args_min = n_args_min;
    o->n_args_max = n_args_max;
    o->fun = fun;
    return o;
}

/******************************************************************************/
/* byte code functions                                                        */

typedef struct _mp_obj_fun_bc_t {
    mp_obj_base_t base;
    mp_map_t *globals;      // the context within which this function was defined
    short n_args;           // number of arguments this function takes
    short n_def_args;       // number of default arguments
    uint n_state;           // total state size for the executing function (incl args, locals, stack)
    const byte *bytecode;   // bytecode for the function
    mp_obj_t def_args[];    // values of default args, if any
} mp_obj_fun_bc_t;

mp_obj_t fun_bc_call(mp_obj_t self_in, uint n_args, uint n_kw, const mp_obj_t *args) {
    mp_obj_fun_bc_t *self = self_in;

    if (n_args < self->n_args - self->n_def_args || n_args > self->n_args) {
        nlr_jump(mp_obj_new_exception_msg_2_args(MP_QSTR_TypeError, "function takes %d positional arguments but %d were given", (const char*)(machine_int_t)self->n_args, (const char*)(machine_int_t)n_args));
    }
    if (n_kw != 0) {
        nlr_jump(mp_obj_new_exception_msg(MP_QSTR_TypeError, "function does not take keyword arguments"));
    }

    mp_obj_t full_args[n_args];
    if (n_args < self->n_args) {
        memcpy(full_args, args, n_args * sizeof(*args));
        int use_def_args = self->n_args - n_args;
        memcpy(full_args + n_args, self->def_args + self->n_def_args - use_def_args, use_def_args * sizeof(*args));
        args = full_args;
        n_args = self->n_args;
    }

    // optimisation: allow the compiler to optimise this tail call for
    // the common case when the globals don't need to be changed
    mp_map_t *old_globals = rt_globals_get();
    if (self->globals == old_globals) {
        return mp_execute_byte_code(self->bytecode, args, n_args, self->n_state);
    } else {
        rt_globals_set(self->globals);
        mp_obj_t result = mp_execute_byte_code(self->bytecode, args, n_args, self->n_state);
        rt_globals_set(old_globals);
        return result;
    }
}

const mp_obj_type_t fun_bc_type = {
    { &mp_const_type },
    "function",
    .call = fun_bc_call,
};

mp_obj_t mp_obj_new_fun_bc(int n_args, mp_obj_t def_args_in, uint n_state, const byte *code) {
    int n_def_args = 0;
    mp_obj_tuple_t *def_args = def_args_in;
    if (def_args != MP_OBJ_NULL) {
        n_def_args = def_args->len;
    }
    mp_obj_fun_bc_t *o = m_new_obj_var(mp_obj_fun_bc_t, mp_obj_t, n_def_args);
    o->base.type = &fun_bc_type;
    o->globals = rt_globals_get();
    o->n_args = n_args;
    o->n_def_args = n_def_args;
    o->n_state = n_state;
    o->bytecode = code;
    if (def_args != MP_OBJ_NULL) {
        memcpy(o->def_args, def_args->items, n_def_args * sizeof(*o->def_args));
    }
    return o;
}

void mp_obj_fun_bc_get(mp_obj_t self_in, int *n_args, uint *n_state, const byte **code) {
    assert(MP_OBJ_IS_TYPE(self_in, &fun_bc_type));
    mp_obj_fun_bc_t *self = self_in;
    *n_args = self->n_args;
    *n_state = self->n_state;
    *code = self->bytecode;
}

/******************************************************************************/
/* inline assembler functions                                                 */

typedef struct _mp_obj_fun_asm_t {
    mp_obj_base_t base;
    int n_args;
    void *fun;
} mp_obj_fun_asm_t;

typedef machine_uint_t (*inline_asm_fun_0_t)();
typedef machine_uint_t (*inline_asm_fun_1_t)(machine_uint_t);
typedef machine_uint_t (*inline_asm_fun_2_t)(machine_uint_t, machine_uint_t);
typedef machine_uint_t (*inline_asm_fun_3_t)(machine_uint_t, machine_uint_t, machine_uint_t);

// convert a Micro Python object to a sensible value for inline asm
machine_uint_t convert_obj_for_inline_asm(mp_obj_t obj) {
    // TODO for byte_array, pass pointer to the array
    if (MP_OBJ_IS_SMALL_INT(obj)) {
        return MP_OBJ_SMALL_INT_VALUE(obj);
    } else if (obj == mp_const_none) {
        return 0;
    } else if (obj == mp_const_false) {
        return 0;
    } else if (obj == mp_const_true) {
        return 1;
    } else if (MP_OBJ_IS_STR(obj)) {
        // pointer to the string (it's probably constant though!)
        uint l;
        return (machine_uint_t)mp_obj_str_get_data(obj, &l);
#if MICROPY_ENABLE_FLOAT
    } else if (MP_OBJ_IS_TYPE(obj, &float_type)) {
        // convert float to int (could also pass in float registers)
        return (machine_int_t)mp_obj_float_get(obj);
#endif
    } else if (MP_OBJ_IS_TYPE(obj, &tuple_type)) {
        // pointer to start of tuple (could pass length, but then could use len(x) for that)
        uint len;
        mp_obj_t *items;
        mp_obj_tuple_get(obj, &len, &items);
        return (machine_uint_t)items;
    } else if (MP_OBJ_IS_TYPE(obj, &list_type)) {
        // pointer to start of list (could pass length, but then could use len(x) for that)
        uint len;
        mp_obj_t *items;
        mp_obj_list_get(obj, &len, &items);
        return (machine_uint_t)items;
    } else {
        // just pass along a pointer to the object
        return (machine_uint_t)obj;
    }
}

// convert a return value from inline asm to a sensible Micro Python object
mp_obj_t convert_val_from_inline_asm(machine_uint_t val) {
    return MP_OBJ_NEW_SMALL_INT(val);
}

mp_obj_t fun_asm_call(mp_obj_t self_in, uint n_args, uint n_kw, const mp_obj_t *args) {
    mp_obj_fun_asm_t *self = self_in;

    if (n_args != self->n_args) {
        nlr_jump(mp_obj_new_exception_msg_2_args(MP_QSTR_TypeError, "function takes %d positional arguments but %d were given", (const char*)(machine_int_t)self->n_args, (const char*)(machine_int_t)n_args));
    }
    if (n_kw != 0) {
        nlr_jump(mp_obj_new_exception_msg(MP_QSTR_TypeError, "function does not take keyword arguments"));
    }

    machine_uint_t ret;
    if (n_args == 0) {
        ret = ((inline_asm_fun_0_t)self->fun)();
    } else if (n_args == 1) {
        ret = ((inline_asm_fun_1_t)self->fun)(convert_obj_for_inline_asm(args[0]));
    } else if (n_args == 2) {
        ret = ((inline_asm_fun_2_t)self->fun)(convert_obj_for_inline_asm(args[0]), convert_obj_for_inline_asm(args[1]));
    } else if (n_args == 3) {
        ret = ((inline_asm_fun_3_t)self->fun)(convert_obj_for_inline_asm(args[0]), convert_obj_for_inline_asm(args[1]), convert_obj_for_inline_asm(args[2]));
    } else {
        assert(0);
        ret = 0;
    }

    return convert_val_from_inline_asm(ret);
}

static const mp_obj_type_t fun_asm_type = {
    { &mp_const_type },
    "function",
    .call = fun_asm_call,
};

mp_obj_t mp_obj_new_fun_asm(uint n_args, void *fun) {
    mp_obj_fun_asm_t *o = m_new_obj(mp_obj_fun_asm_t);
    o->base.type = &fun_asm_type;
    o->n_args = n_args;
    o->fun = fun;
    return o;
}
