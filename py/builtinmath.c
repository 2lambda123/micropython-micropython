#include <math.h>

#include "misc.h"
#include "mpconfig.h"
#include "qstr.h"
#include "obj.h"
#include "map.h"
#include "builtin.h"

#if MICROPY_ENABLE_FLOAT

#define MATH_FUN_1(py_name, c_name) \
    mp_obj_t mp_math_ ## py_name(mp_obj_t x_obj) { return mp_obj_new_float(MICROPY_FLOAT_C_FUN(c_name)(mp_obj_get_float(x_obj))); } \
    STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_math_## py_name ## _obj, mp_math_ ## py_name);

#define MATH_FUN_2(py_name, c_name) \
    mp_obj_t mp_math_ ## py_name(mp_obj_t x_obj, mp_obj_t y_obj) { return mp_obj_new_float(MICROPY_FLOAT_C_FUN(c_name)(mp_obj_get_float(x_obj), mp_obj_get_float(y_obj))); } \
    STATIC MP_DEFINE_CONST_FUN_OBJ_2(mp_math_## py_name ## _obj, mp_math_ ## py_name);

STATIC const mp_obj_float_t mp_math_pi_obj = {{&mp_type_float}, M_PI};

MATH_FUN_1(sqrt, sqrt)
MATH_FUN_2(pow, pow)
MATH_FUN_1(exp, exp)
MATH_FUN_1(expm1, expm1)
MATH_FUN_1(log, log)
MATH_FUN_1(log2, log2)
MATH_FUN_1(log10, log10)
MATH_FUN_1(cosh, cosh)
MATH_FUN_1(sinh, sinh)
MATH_FUN_1(tanh, tanh)
MATH_FUN_1(acosh, acosh)
MATH_FUN_1(asinh, asinh)
MATH_FUN_1(atanh, atanh)
MATH_FUN_1(cos, cos)
MATH_FUN_1(sin, sin)
MATH_FUN_1(tan, tan)
MATH_FUN_1(acos, acos)
MATH_FUN_1(asin, asin)
MATH_FUN_1(atan, atan)
MATH_FUN_2(atan2, atan2)

STATIC const mp_map_elem_t mp_module_math_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_math) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_pi), (mp_obj_t)&mp_math_pi_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sqrt), (mp_obj_t)&mp_math_sqrt_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_pow), (mp_obj_t)&mp_math_pow_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_exp), (mp_obj_t)&mp_math_exp_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_expm1), (mp_obj_t)&mp_math_expm1_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_log), (mp_obj_t)&mp_math_log_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_log2), (mp_obj_t)&mp_math_log2_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_log10), (mp_obj_t)&mp_math_log10_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_cosh), (mp_obj_t)&mp_math_cosh_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sinh), (mp_obj_t)&mp_math_sinh_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_tanh), (mp_obj_t)&mp_math_tanh_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_acosh), (mp_obj_t)&mp_math_acosh_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_asinh), (mp_obj_t)&mp_math_asinh_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_atanh), (mp_obj_t)&mp_math_atanh_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_cos), (mp_obj_t)&mp_math_cos_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sin), (mp_obj_t)&mp_math_sin_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_tan), (mp_obj_t)&mp_math_tan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_acos), (mp_obj_t)&mp_math_acos_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_asin), (mp_obj_t)&mp_math_asin_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_atan), (mp_obj_t)&mp_math_atan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_atan2), (mp_obj_t)&mp_math_atan2_obj },
};

STATIC const mp_map_t mp_module_math_globals = {
    .all_keys_are_qstrs = 1,
    .table_is_fixed_array = 1,
    .used = sizeof(mp_module_math_globals_table) / sizeof(mp_map_elem_t),
    .alloc = sizeof(mp_module_math_globals_table) / sizeof(mp_map_elem_t),
    .table = (mp_map_elem_t*)mp_module_math_globals_table,
};

const mp_obj_module_t mp_module_math = {
    .base = { &mp_type_module },
    .name = MP_QSTR_math,
    .globals = (mp_map_t*)&mp_module_math_globals,
};

#endif // MICROPY_ENABLE_FLOAT
