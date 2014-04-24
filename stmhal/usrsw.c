#include <stdio.h>

#include "stm32f4xx_hal.h"

#include "misc.h"
#include "mpconfig.h"
#include "qstr.h"
#include "obj.h"
#include "runtime.h"
#include "extint.h"
#include "pin.h"
#include "genhdr/pins.h"
#include "usrsw.h"

#if MICROPY_HW_HAS_SWITCH

// Usage Model:
//
//      sw = pyb.Switch()       # create a switch object
//      sw()                    # get state (True if pressed, False otherwise)
//      sw.callback(f)          # register a callback to be called when the
//                              #   switch is pressed down
//      sw.callback(None)       # remove the callback
//
// Example:
//
// pyb.Switch().callback(lambda: pyb.LED(1).toggle())

// this function inits the switch GPIO so that it can be used
void switch_init0(void) {
    GPIO_InitTypeDef init;
    init.Pin = MICROPY_HW_USRSW_PIN.pin_mask;
    init.Mode = GPIO_MODE_INPUT;
    init.Pull = MICROPY_HW_USRSW_PULL;
    init.Speed = GPIO_SPEED_FAST;
    HAL_GPIO_Init(MICROPY_HW_USRSW_PIN.gpio, &init);
}

int switch_get(void) {
    int val = ((MICROPY_HW_USRSW_PIN.gpio->IDR & MICROPY_HW_USRSW_PIN.pin_mask) != 0);
    return val == MICROPY_HW_USRSW_PRESSED;
}

/******************************************************************************/
// Micro Python bindings

typedef struct _pyb_switch_obj_t {
    mp_obj_base_t base;
    mp_obj_t callback;
} pyb_switch_obj_t;

STATIC pyb_switch_obj_t pyb_switch_obj;

void pyb_switch_print(void (*print)(void *env, const char *fmt, ...), void *env, mp_obj_t self_in, mp_print_kind_t kind) {
    print(env, "Switch()");
}

STATIC mp_obj_t pyb_switch_make_new(mp_obj_t type_in, uint n_args, uint n_kw, const mp_obj_t *args) {
    // check arguments
    mp_arg_check_num(n_args, n_kw, 0, 0, false);

    // init the switch object
    pyb_switch_obj.base.type = &pyb_switch_type;

    // No need to clear the callback member: if it's already been set and registered
    // with extint then we don't want to reset that behaviour.  If it hasn't been set,
    // then no extint will be called until it is set via the callback method.

    // return static switch object
    return (mp_obj_t)&pyb_switch_obj;
}

mp_obj_t pyb_switch_call(mp_obj_t self_in, uint n_args, uint n_kw, const mp_obj_t *args) {
    // get switch state
    mp_arg_check_num(n_args, n_kw, 0, 0, false);
    return switch_get() ? mp_const_true : mp_const_false;
}

STATIC mp_obj_t switch_callback(mp_obj_t line) {
    if (pyb_switch_obj.callback != mp_const_none) {
        mp_call_function_0(pyb_switch_obj.callback);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(switch_callback_obj, switch_callback);

mp_obj_t pyb_switch_callback(mp_obj_t self_in, mp_obj_t callback) {
    pyb_switch_obj_t *self = self_in;
    self->callback = callback;
    // Init the EXTI each time this function is called, since the EXTI
    // may have been disabled by an exception in the interrupt, or the
    // user disabling the line explicitly.
    extint_register((mp_obj_t)&MICROPY_HW_USRSW_PIN,
                    MICROPY_HW_USRSW_EXTI_MODE,
                    MICROPY_HW_USRSW_PULL,
                    callback == mp_const_none ? mp_const_none : (mp_obj_t)&switch_callback_obj,
                    true, NULL);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(pyb_switch_callback_obj, pyb_switch_callback);

STATIC const mp_map_elem_t pyb_switch_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_callback), (mp_obj_t)&pyb_switch_callback_obj },
};

STATIC MP_DEFINE_CONST_DICT(pyb_switch_locals_dict, pyb_switch_locals_dict_table);

const mp_obj_type_t pyb_switch_type = {
    { &mp_type_type },
    .name = MP_QSTR_Switch,
    .print = pyb_switch_print,
    .make_new = pyb_switch_make_new,
    .call = pyb_switch_call,
    .locals_dict = (mp_obj_t)&pyb_switch_locals_dict,
};

#endif // MICROPY_HW_HAS_SWITCH
