// options to control how Micro Python is built

#define MICROPY_ENABLE_FLOAT        (1)
#define MICROPY_EMIT_CPYTHON        (0)
#define MICROPY_EMIT_X64            (1)
#define MICROPY_EMIT_THUMB          (0)
#define MICROPY_EMIT_INLINE_THUMB   (0)

// type definitions for the specific machine

#ifdef __LP64__
typedef long machine_int_t; // must be pointer size
typedef unsigned long machine_uint_t; // must be pointer size
#define UINT_FMT "%lu"
#define INT_FMT "%ld"
#else
// These are definitions for machines where sizeof(int) == sizeof(void*),
// regardless for actual size.
typedef int machine_int_t; // must be pointer size
typedef unsigned int machine_uint_t; // must be pointer size
#define UINT_FMT "%u"
#define INT_FMT "%d"
#endif

#define BYTES_PER_WORD sizeof(machine_int_t)

typedef void *machine_ptr_t; // must be of pointer size
typedef const void *machine_const_ptr_t; // must be of pointer size
typedef double machine_float_t;

machine_float_t machine_sqrt(machine_float_t x);
