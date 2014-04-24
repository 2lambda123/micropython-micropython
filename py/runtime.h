typedef enum {
    MP_VM_RETURN_NORMAL,
    MP_VM_RETURN_YIELD,
    MP_VM_RETURN_EXCEPTION,
} mp_vm_return_kind_t;

typedef enum {
    MP_ARG_PARSE_BOOL      = 0x001,
    MP_ARG_PARSE_INT       = 0x002,
    MP_ARG_PARSE_OBJ       = 0x003,
    MP_ARG_PARSE_KIND_MASK = 0x0ff,
    MP_ARG_PARSE_REQUIRED  = 0x100,
    MP_ARG_PARSE_KW_ONLY   = 0x200,
} mp_arg_parse_flag_t;

typedef union _mp_arg_parse_val_t {
    bool u_bool;
    machine_int_t u_int;
    mp_obj_t u_obj;
} mp_arg_parse_val_t;

typedef struct _mp_arg_parse_t {
    qstr qstr;
    machine_uint_t flags;
    mp_arg_parse_val_t defval;
} mp_arg_parse_t;

void mp_init(void);
void mp_deinit(void);

void mp_arg_check_num(uint n_args, uint n_kw, uint n_args_min, uint n_args_max, bool takes_kw);
void mp_arg_parse_all(uint n_pos, const mp_obj_t *pos, mp_map_t *kws, uint n_allowed, const mp_arg_parse_t *allowed, mp_arg_parse_val_t *out_vals);

mp_obj_dict_t *mp_locals_get(void);
void mp_locals_set(mp_obj_dict_t *d);
mp_obj_dict_t *mp_globals_get(void);
void mp_globals_set(mp_obj_dict_t *d);

mp_obj_t mp_load_name(qstr qstr);
mp_obj_t mp_load_global(qstr qstr);
mp_obj_t mp_load_build_class(void);
void mp_store_name(qstr qstr, mp_obj_t obj);
void mp_store_global(qstr qstr, mp_obj_t obj);
void mp_delete_name(qstr qstr);
void mp_delete_global(qstr qstr);

mp_obj_t mp_unary_op(int op, mp_obj_t arg);
mp_obj_t mp_binary_op(int op, mp_obj_t lhs, mp_obj_t rhs);

mp_obj_t mp_load_const_dec(qstr qstr);
mp_obj_t mp_load_const_str(qstr qstr);
mp_obj_t mp_load_const_bytes(qstr qstr);

mp_obj_t mp_make_function_n(int n_args, void *fun); // fun must have the correct signature for n_args fixed arguments
mp_obj_t mp_make_function_var(int n_args_min, mp_fun_var_t fun);
mp_obj_t mp_make_function_var_between(int n_args_min, int n_args_max, mp_fun_var_t fun); // min and max are inclusive

mp_obj_t mp_call_function_0(mp_obj_t fun);
mp_obj_t mp_call_function_1(mp_obj_t fun, mp_obj_t arg);
mp_obj_t mp_call_function_2(mp_obj_t fun, mp_obj_t arg1, mp_obj_t arg2);
mp_obj_t mp_call_function_n_kw_for_native(mp_obj_t fun_in, uint n_args_kw, const mp_obj_t *args);
mp_obj_t mp_call_function_n_kw(mp_obj_t fun, uint n_args, uint n_kw, const mp_obj_t *args);
mp_obj_t mp_call_method_n_kw(uint n_args, uint n_kw, const mp_obj_t *args);
mp_obj_t mp_call_method_n_kw_var(bool have_self, uint n_args_n_kw, const mp_obj_t *args);

void mp_unpack_sequence(mp_obj_t seq, uint num, mp_obj_t *items);
void mp_unpack_ex(mp_obj_t seq, uint num, mp_obj_t *items);
mp_obj_t mp_store_map(mp_obj_t map, mp_obj_t key, mp_obj_t value);
mp_obj_t mp_load_attr(mp_obj_t base, qstr attr);
void mp_load_method(mp_obj_t base, qstr attr, mp_obj_t *dest);
void mp_load_method_maybe(mp_obj_t base, qstr attr, mp_obj_t *dest);
void mp_store_attr(mp_obj_t base, qstr attr, mp_obj_t val);

mp_obj_t mp_getiter(mp_obj_t o);
mp_obj_t mp_iternext_allow_raise(mp_obj_t o); // may return MP_OBJ_STOP_ITERATION instead of raising StopIteration()
mp_obj_t mp_iternext(mp_obj_t o); // will always return MP_OBJ_STOP_ITERATION instead of raising StopIteration(...)
mp_vm_return_kind_t mp_resume(mp_obj_t self_in, mp_obj_t send_value, mp_obj_t throw_value, mp_obj_t *ret_val);

mp_obj_t mp_make_raise_obj(mp_obj_t o);

mp_map_t *mp_loaded_modules_get(void);
mp_obj_t mp_import_name(qstr name, mp_obj_t fromlist, mp_obj_t level);
mp_obj_t mp_import_from(mp_obj_t module, qstr name);
void mp_import_all(mp_obj_t module);

extern struct _mp_obj_list_t mp_sys_path_obj;
extern struct _mp_obj_list_t mp_sys_argv_obj;
#define mp_sys_path ((mp_obj_t)&mp_sys_path_obj)
#define mp_sys_argv ((mp_obj_t)&mp_sys_argv_obj)
