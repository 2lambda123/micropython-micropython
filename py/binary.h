// Use special typecode to differentiate repr() of bytearray vs array.array('B')
// (underlyingly they're same).
#define BYTEARRAY_TYPECODE 0

int mp_binary_get_size(char struct_type, char val_type, uint *palign);
mp_obj_t mp_binary_get_val_array(char typecode, void *p, int index);
void mp_binary_set_val_array(char typecode, void *p, int index, mp_obj_t val_in);
void mp_binary_set_val_array_from_int(char typecode, void *p, int index, machine_int_t val);
mp_obj_t mp_binary_get_val(char struct_type, char val_type, byte **ptr);
void mp_binary_set_val(char struct_type, char val_type, mp_obj_t val_in, byte **ptr);
