typedef struct _mp_obj_int_t {
    mp_obj_base_t base;
#if MICROPY_LONGINT_IMPL != MICROPY_LONGINT_IMPL_NONE
    mp_longint_impl_t val;
#endif
} mp_obj_int_t;

void int_print(void (*print)(void *env, const char *fmt, ...), void *env, mp_obj_t self_in, mp_print_kind_t kind);
mp_obj_t int_binary_op(int op, mp_obj_t lhs_in, mp_obj_t rhs_in);
