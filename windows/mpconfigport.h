// options to control how Micro Python is built

// Linking with GNU readline causes binary to be licensed under GPL
#ifndef MICROPY_USE_READLINE
#define MICROPY_USE_READLINE        (0)
#endif

#define MICROPY_EMIT_X64            (0)
#define MICROPY_EMIT_THUMB          (0)
#define MICROPY_EMIT_INLINE_THUMB   (0)
#define MICROPY_MEM_STATS           (1)
#define MICROPY_DEBUG_PRINTERS      (1)
#define MICROPY_ENABLE_REPL_HELPERS (1)
#define MICROPY_ENABLE_LEXER_UNIX   (1)
#define MICROPY_MOD_SYS_STDFILES    (1)
#define MICROPY_FLOAT_IMPL          (MICROPY_FLOAT_IMPL_DOUBLE)
#define MICROPY_LONGINT_IMPL        (MICROPY_LONGINT_IMPL_MPZ)
#define MICROPY_PORT_INIT_FUNC      init()

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

#include "realpath.h"
#include "init.h"
