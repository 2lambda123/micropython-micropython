// options to control how Micro Python is built

#define MICROPY_EMIT_X64            (1)
#define MICROPY_EMIT_THUMB          (0)
#define MICROPY_EMIT_INLINE_THUMB   (0)
#define MICROPY_ENABLE_GC           (1)
#define MICROPY_ENABLE_FINALISER    (1)
#define MICROPY_MEM_STATS           (1)
#define MICROPY_DEBUG_PRINTERS      (1)
#define MICROPY_ENABLE_REPL_HELPERS (1)
#define MICROPY_ENABLE_LEXER_UNIX   (1)
#define MICROPY_ENABLE_SOURCE_LINE  (1)
#define MICROPY_FLOAT_IMPL          (MICROPY_FLOAT_IMPL_DOUBLE)
#define MICROPY_LONGINT_IMPL        (MICROPY_LONGINT_IMPL_MPZ)
#define MICROPY_PATH_MAX            (PATH_MAX)
#define MICROPY_USE_COMPUTED_GOTO   (1)
#define MICROPY_MOD_SYS_STDFILES    (1)
#define MICROPY_ENABLE_MOD_CMATH    (1)
// Define to MICROPY_ERROR_REPORTING_DETAILED to get function, etc.
// names in exception messages (may require more RAM).
#define MICROPY_ERROR_REPORTING     (MICROPY_ERROR_REPORTING_DETAILED)

extern const struct _mp_obj_module_t mp_module_time;
extern const struct _mp_obj_module_t mp_module_socket;
extern const struct _mp_obj_module_t mp_module_ffi;
#define MICROPY_EXTRA_BUILTIN_MODULES \
    { MP_OBJ_NEW_QSTR(MP_QSTR_ffi), (mp_obj_t)&mp_module_ffi }, \
    { MP_OBJ_NEW_QSTR(MP_QSTR_time), (mp_obj_t)&mp_module_time }, \
    { MP_OBJ_NEW_QSTR(MP_QSTR_microsocket), (mp_obj_t)&mp_module_socket }, \

// type definitions for the specific machine

#ifdef __LP64__
typedef long machine_int_t; // must be pointer size
typedef unsigned long machine_uint_t; // must be pointer size
#else
// These are definitions for machines where sizeof(int) == sizeof(void*),
// regardless for actual size.
typedef int machine_int_t; // must be pointer size
typedef unsigned int machine_uint_t; // must be pointer size
#endif

#define BYTES_PER_WORD sizeof(machine_int_t)

typedef void *machine_ptr_t; // must be of pointer size
typedef const void *machine_const_ptr_t; // must be of pointer size

extern const struct _mp_obj_fun_native_t mp_builtin_open_obj;
#define MICROPY_EXTRA_BUILTINS \
    { MP_OBJ_NEW_QSTR(MP_QSTR_open), (mp_obj_t)&mp_builtin_open_obj },
