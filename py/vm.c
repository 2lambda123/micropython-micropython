/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mpconfig.h"
#include "nlr.h"
#include "misc.h"
#include "qstr.h"
#include "obj.h"
#include "emitglue.h"
#include "runtime.h"
#include "bc0.h"
#include "bc.h"
#include "objgenerator.h"

// With these macros you can tune the maximum number of state slots
// that will be allocated on the stack.  Any function that needs more
// than this will use the heap.
#define VM_MAX_STATE_ON_STACK (10)
#define VM_MAX_EXC_STATE_ON_STACK (4)

#define DETECT_VM_STACK_OVERFLOW (0)
#if 0
#define TRACE(ip) mp_bytecode_print2(ip, 1);
#else
#define TRACE(ip)
#endif

// Value stack grows up (this makes it incompatible with native C stack, but
// makes sure that arguments to functions are in natural order arg1..argN
// (Python semantics mandates left-to-right evaluation order, including for
// function arguments). Stack pointer is pre-incremented and points at the
// top element.
// Exception stack also grows up, top element is also pointed at.

// Exception stack unwind reasons (WHY_* in CPython-speak)
// TODO perhaps compress this to RETURN=0, JUMP>0, with number of unwinds
// left to do encoded in the JUMP number
typedef enum {
    UNWIND_RETURN = 1,
    UNWIND_JUMP,
} mp_unwind_reason_t;

#define DECODE_UINT do { \
    unum = 0; \
    do { \
        unum = (unum << 7) + (*ip & 0x7f); \
    } while ((*ip++ & 0x80) != 0); \
} while (0)
#define DECODE_ULABEL do { unum = (ip[0] | (ip[1] << 8)); ip += 2; } while (0)
#define DECODE_SLABEL do { unum = (ip[0] | (ip[1] << 8)) - 0x8000; ip += 2; } while (0)
#define DECODE_QSTR do { \
    qst = 0; \
    do { \
        qst = (qst << 7) + (*ip & 0x7f); \
    } while ((*ip++ & 0x80) != 0); \
} while (0)
#define DECODE_PTR do { \
    ip = (byte*)(((machine_uint_t)ip + sizeof(machine_uint_t) - 1) & (~(sizeof(machine_uint_t) - 1))); /* align ip */ \
    unum = *(machine_uint_t*)ip; \
    ip += sizeof(machine_uint_t); \
} while (0)
#define PUSH(val) *++sp = (val)
#define POP() (*sp--)
#define TOP() (*sp)
#define SET_TOP(val) *sp = (val)

#define PUSH_EXC_BLOCK() \
    DECODE_ULABEL; /* except labels are always forward */ \
    ++exc_sp; \
    exc_sp->opcode = *save_ip; \
    exc_sp->handler = ip + unum; \
    exc_sp->val_sp = MP_TAGPTR_MAKE(sp, currently_in_except_block); \
    exc_sp->prev_exc = MP_OBJ_NULL; \
    currently_in_except_block = 0; /* in a try block now */

#define POP_EXC_BLOCK() \
    currently_in_except_block = MP_TAGPTR_TAG(exc_sp->val_sp); /* restore previous state */ \
    exc_sp--; /* pop back to previous exception handler */

mp_vm_return_kind_t mp_execute_bytecode(const byte *code, const mp_obj_t *args, uint n_args, const mp_obj_t *args2, uint n_args2, mp_obj_t *ret) {
    const byte *ip = code;

    // get code info size, and skip line number table
    machine_uint_t code_info_size = ip[0] | (ip[1] << 8) | (ip[2] << 16) | (ip[3] << 24);
    ip += code_info_size;

    // bytecode prelude: state size and exception stack size; 16 bit uints
    machine_uint_t n_state = ip[0] | (ip[1] << 8);
    machine_uint_t n_exc_stack = ip[2] | (ip[3] << 8);
    ip += 4;

    // allocate state for locals and stack
    mp_obj_t temp_state[VM_MAX_STATE_ON_STACK];
    mp_obj_t *state = &temp_state[0];
#if DETECT_VM_STACK_OVERFLOW
    n_state += 1;
#endif
    if (n_state > VM_MAX_STATE_ON_STACK) {
        state = m_new(mp_obj_t, n_state);
    }
    mp_obj_t *sp = &state[0] - 1;

    // allocate state for exceptions
    mp_exc_stack_t exc_state[VM_MAX_EXC_STATE_ON_STACK];
    mp_exc_stack_t *exc_stack = &exc_state[0];
    if (n_exc_stack > VM_MAX_EXC_STATE_ON_STACK) {
        exc_stack = m_new(mp_exc_stack_t, n_exc_stack);
    }
    mp_exc_stack_t *exc_sp = &exc_stack[0] - 1;

    // init args
    for (uint i = 0; i < n_args; i++) {
        state[n_state - 1 - i] = args[i];
    }
    for (uint i = 0; i < n_args2; i++) {
        state[n_state - 1 - n_args - i] = args2[i];
    }

    // set rest of state to MP_OBJ_NULL
    for (uint i = 0; i < n_state - n_args - n_args2; i++) {
        state[i] = MP_OBJ_NULL;
    }

    // bytecode prelude: initialise closed over variables
    for (uint n_local = *ip++; n_local > 0; n_local--) {
        uint local_num = *ip++;
        state[n_state - 1 - local_num] = mp_obj_new_cell(state[n_state - 1 - local_num]);
    }

    // execute the byte code
    mp_vm_return_kind_t vm_return_kind = mp_execute_bytecode2(code, &ip, &state[n_state - 1], &sp, exc_stack, &exc_sp, MP_OBJ_NULL);

#if DETECT_VM_STACK_OVERFLOW
    // We can't check the case when an exception is returned in state[n_state - 1]
    // and there are no arguments, because in this case our detection slot may have
    // been overwritten by the returned exception (which is allowed).
    if (!(vm_return_kind == MP_VM_RETURN_EXCEPTION && n_args == 0 && n_args2 == 0)) {
        // Just check to see that we have at least 1 null object left in the state.
        bool overflow = true;
        for (uint i = 0; i < n_state - n_args - n_args2; i++) {
            if (state[i] == MP_OBJ_NULL) {
                overflow = false;
                break;
            }
        }
        if (overflow) {
            printf("VM stack overflow state=%p n_state+1=%u\n", state, n_state);
            assert(0);
        }
    }
#endif

    mp_vm_return_kind_t ret_kind;
    switch (vm_return_kind) {
        case MP_VM_RETURN_NORMAL:
            // return value is in *sp
            *ret = *sp;
            ret_kind = MP_VM_RETURN_NORMAL;
            break;

        case MP_VM_RETURN_EXCEPTION:
            // return value is in state[n_state - 1]
            *ret = state[n_state - 1];
            ret_kind = MP_VM_RETURN_EXCEPTION;
            break;

        case MP_VM_RETURN_YIELD: // byte-code shouldn't yield
        default:
            assert(0);
            *ret = mp_const_none;
            ret_kind = MP_VM_RETURN_NORMAL;
    }

    // free the state if it was allocated on the heap
    if (n_state > VM_MAX_STATE_ON_STACK) {
        m_free(state, n_state);
    }

    // free the exception state if it was allocated on the heap
    if (n_exc_stack > VM_MAX_EXC_STATE_ON_STACK) {
        m_free(exc_stack, n_exc_stack);
    }

    return ret_kind;
}

// fastn has items in reverse order (fastn[0] is local[0], fastn[-1] is local[1], etc)
// sp points to bottom of stack which grows up
// returns:
//  MP_VM_RETURN_NORMAL, sp valid, return value in *sp
//  MP_VM_RETURN_YIELD, ip, sp valid, yielded value in *sp
//  MP_VM_RETURN_EXCEPTION, exception in fastn[0]
mp_vm_return_kind_t mp_execute_bytecode2(const byte *code_info, const byte **ip_in_out,
                                         mp_obj_t *fastn, mp_obj_t **sp_in_out,
                                         mp_exc_stack_t *exc_stack, mp_exc_stack_t **exc_sp_in_out,
                                         volatile mp_obj_t inject_exc) {
#if MICROPY_USE_COMPUTED_GOTO
    #include "vmentrytable.h"
    #define DISPATCH() do { \
        TRACE(ip); \
        save_ip = ip; \
        goto *entry_table[*ip++]; \
    } while(0)
    #define ENTRY(op) entry_##op
    #define ENTRY_DEFAULT entry_default
#else
    #define DISPATCH() break
    #define ENTRY(op) case op
    #define ENTRY_DEFAULT default
#endif

    // nlr_raise needs to be implemented as a goto, so that the C compiler's flow analyser
    // sees that it's possible for us to jump from the dispatch loop to the exception
    // handler.  Without this, the code may have a different stack layout in the dispatch
    // loop and the exception handler, leading to very obscure bugs.
    #define RAISE(o) do { nlr_pop(); nlr.ret_val = o; goto exception_handler; } while(0)

    // variables that are visible to the exception handler (declared volatile)
    volatile bool currently_in_except_block = MP_TAGPTR_TAG(*exc_sp_in_out); // 0 or 1, to detect nested exceptions
    mp_exc_stack_t *volatile exc_sp = MP_TAGPTR_PTR(*exc_sp_in_out); // stack grows up, exc_sp points to top of stack
    const byte *volatile save_ip = *ip_in_out; // this is so we can access ip in the exception handler without making ip volatile (which means the compiler can't keep it in a register in the main loop)
    mp_obj_t *volatile save_sp = *sp_in_out; // this is so we can access sp in the exception handler when needed

    // outer exception handling loop
    for (;;) {
        nlr_buf_t nlr;
outer_dispatch_loop:
        if (nlr_push(&nlr) == 0) {
            // local variables that are not visible to the exception handler
            const byte *ip = *ip_in_out;
            mp_obj_t *sp = *sp_in_out;
            machine_uint_t unum;
            qstr qst;
            mp_obj_t obj1, obj2;

            // If we have exception to inject, now that we finish setting up
            // execution context, raise it. This works as if RAISE_VARARGS
            // bytecode was executed.
            // Injecting exc into yield from generator is a special case,
            // handled by MP_BC_YIELD_FROM itself
            if (inject_exc != MP_OBJ_NULL && *ip != MP_BC_YIELD_FROM) {
                obj1 = inject_exc;
                inject_exc = MP_OBJ_NULL;
                obj1 = mp_make_raise_obj(obj1);
                RAISE(obj1);
            }

            // loop to execute byte code
            for (;;) {
dispatch_loop:
#if MICROPY_USE_COMPUTED_GOTO
                DISPATCH();
#else
                TRACE(ip);
                save_ip = ip;
                switch (*ip++) {
#endif

                ENTRY(MP_BC_LOAD_CONST_FALSE):
                    PUSH(mp_const_false);
                    DISPATCH();

                ENTRY(MP_BC_LOAD_CONST_NONE):
                    PUSH(mp_const_none);
                    DISPATCH();

                ENTRY(MP_BC_LOAD_CONST_TRUE):
                    PUSH(mp_const_true);
                    DISPATCH();

                ENTRY(MP_BC_LOAD_CONST_ELLIPSIS):
                    PUSH((mp_obj_t)&mp_const_ellipsis_obj);
                    DISPATCH();

                ENTRY(MP_BC_LOAD_CONST_SMALL_INT): {
                    machine_int_t num = 0;
                    if ((ip[0] & 0x40) != 0) {
                        // Number is negative
                        num--;
                    }
                    do {
                        num = (num << 7) | (*ip & 0x7f);
                    } while ((*ip++ & 0x80) != 0);
                    PUSH(MP_OBJ_NEW_SMALL_INT(num));
                    DISPATCH();
                }

                ENTRY(MP_BC_LOAD_CONST_INT):
                    DECODE_QSTR;
                    PUSH(mp_obj_new_int_from_qstr(qst));
                    DISPATCH();

                ENTRY(MP_BC_LOAD_CONST_DEC):
                    DECODE_QSTR;
                    PUSH(mp_load_const_dec(qst));
                    DISPATCH();

                ENTRY(MP_BC_LOAD_CONST_BYTES):
                    DECODE_QSTR;
                    PUSH(mp_load_const_bytes(qst));
                    DISPATCH();

                ENTRY(MP_BC_LOAD_CONST_STRING):
                    DECODE_QSTR;
                    PUSH(mp_load_const_str(qst));
                    DISPATCH();

                ENTRY(MP_BC_LOAD_NULL):
                    PUSH(MP_OBJ_NULL);
                    DISPATCH();

                ENTRY(MP_BC_LOAD_FAST_0):
                    obj1 = fastn[0];
                    goto load_check;

                ENTRY(MP_BC_LOAD_FAST_1):
                    obj1 = fastn[-1];
                    goto load_check;

                ENTRY(MP_BC_LOAD_FAST_2):
                    obj1 = fastn[-2];
                    goto load_check;

                ENTRY(MP_BC_LOAD_FAST_N):
                    DECODE_UINT;
                    obj1 = fastn[-unum];
                    load_check:
                    if (obj1 == MP_OBJ_NULL) {
                        local_name_error:
                        obj1 = mp_obj_new_exception_msg(&mp_type_NameError, "local variable referenced before assignment");
                        RAISE(obj1);
                    }
                    PUSH(obj1);
                    DISPATCH();

                ENTRY(MP_BC_LOAD_DEREF):
                    DECODE_UINT;
                    obj1 = mp_obj_cell_get(fastn[-unum]);
                    goto load_check;

                ENTRY(MP_BC_LOAD_NAME):
                    DECODE_QSTR;
                    PUSH(mp_load_name(qst));
                    DISPATCH();

                ENTRY(MP_BC_LOAD_GLOBAL):
                    DECODE_QSTR;
                    PUSH(mp_load_global(qst));
                    DISPATCH();

                ENTRY(MP_BC_LOAD_ATTR):
                    DECODE_QSTR;
                    SET_TOP(mp_load_attr(TOP(), qst));
                    DISPATCH();

                ENTRY(MP_BC_LOAD_METHOD):
                    DECODE_QSTR;
                    mp_load_method(*sp, qst, sp);
                    sp += 1;
                    DISPATCH();

                ENTRY(MP_BC_LOAD_BUILD_CLASS):
                    PUSH(mp_load_build_class());
                    DISPATCH();

                ENTRY(MP_BC_LOAD_SUBSCR):
                    obj1 = POP();
                    SET_TOP(mp_obj_subscr(TOP(), obj1, MP_OBJ_SENTINEL));
                    DISPATCH();

                ENTRY(MP_BC_STORE_FAST_0):
                    fastn[0] = POP();
                    DISPATCH();

                ENTRY(MP_BC_STORE_FAST_1):
                    fastn[-1] = POP();
                    DISPATCH();

                ENTRY(MP_BC_STORE_FAST_2):
                    fastn[-2] = POP();
                    DISPATCH();

                ENTRY(MP_BC_STORE_FAST_N):
                    DECODE_UINT;
                    fastn[-unum] = POP();
                    DISPATCH();

                ENTRY(MP_BC_STORE_DEREF):
                    DECODE_UINT;
                    mp_obj_cell_set(fastn[-unum], POP());
                    DISPATCH();

                ENTRY(MP_BC_STORE_NAME):
                    DECODE_QSTR;
                    mp_store_name(qst, POP());
                    DISPATCH();

                ENTRY(MP_BC_STORE_GLOBAL):
                    DECODE_QSTR;
                    mp_store_global(qst, POP());
                    DISPATCH();

                ENTRY(MP_BC_STORE_ATTR):
                    DECODE_QSTR;
                    mp_store_attr(sp[0], qst, sp[-1]);
                    sp -= 2;
                    DISPATCH();

                ENTRY(MP_BC_STORE_SUBSCR):
                    mp_obj_subscr(sp[-1], sp[0], sp[-2]);
                    sp -= 3;
                    DISPATCH();

                ENTRY(MP_BC_DELETE_FAST):
                    DECODE_UINT;
                    if (fastn[-unum] == MP_OBJ_NULL) {
                        goto local_name_error;
                    }
                    fastn[-unum] = MP_OBJ_NULL;
                    DISPATCH();

                ENTRY(MP_BC_DELETE_DEREF):
                    DECODE_UINT;
                    if (mp_obj_cell_get(fastn[-unum]) == MP_OBJ_NULL) {
                        goto local_name_error;
                    }
                    mp_obj_cell_set(fastn[-unum], MP_OBJ_NULL);
                    DISPATCH();

                ENTRY(MP_BC_DELETE_NAME):
                    DECODE_QSTR;
                    mp_delete_name(qst);
                    DISPATCH();

                ENTRY(MP_BC_DELETE_GLOBAL):
                    DECODE_QSTR;
                    mp_delete_global(qst);
                    DISPATCH();

                ENTRY(MP_BC_DUP_TOP):
                    obj1 = TOP();
                    PUSH(obj1);
                    DISPATCH();

                ENTRY(MP_BC_DUP_TOP_TWO):
                    sp += 2;
                    sp[0] = sp[-2];
                    sp[-1] = sp[-3];
                    DISPATCH();

                ENTRY(MP_BC_POP_TOP):
                    sp -= 1;
                    DISPATCH();

                ENTRY(MP_BC_ROT_TWO):
                    obj1 = sp[0];
                    sp[0] = sp[-1];
                    sp[-1] = obj1;
                    DISPATCH();

                ENTRY(MP_BC_ROT_THREE):
                    obj1 = sp[0];
                    sp[0] = sp[-1];
                    sp[-1] = sp[-2];
                    sp[-2] = obj1;
                    DISPATCH();

                ENTRY(MP_BC_JUMP):
                    DECODE_SLABEL;
                    ip += unum;
                    DISPATCH();

                ENTRY(MP_BC_POP_JUMP_IF_TRUE):
                    DECODE_SLABEL;
                    if (mp_obj_is_true(POP())) {
                        ip += unum;
                    }
                    DISPATCH();

                ENTRY(MP_BC_POP_JUMP_IF_FALSE):
                    DECODE_SLABEL;
                    if (!mp_obj_is_true(POP())) {
                        ip += unum;
                    }
                    DISPATCH();

                ENTRY(MP_BC_JUMP_IF_TRUE_OR_POP):
                    DECODE_SLABEL;
                    if (mp_obj_is_true(TOP())) {
                        ip += unum;
                    } else {
                        sp--;
                    }
                    DISPATCH();

                ENTRY(MP_BC_JUMP_IF_FALSE_OR_POP):
                    DECODE_SLABEL;
                    if (mp_obj_is_true(TOP())) {
                        sp--;
                    } else {
                        ip += unum;
                    }
                    DISPATCH();

                ENTRY(MP_BC_SETUP_WITH):
                    obj1 = TOP();
                    SET_TOP(mp_load_attr(obj1, MP_QSTR___exit__));
                    mp_load_method(obj1, MP_QSTR___enter__, sp + 1);
                    obj2 = mp_call_method_n_kw(0, 0, sp + 1);
                    PUSH_EXC_BLOCK();
                    PUSH(obj2);
                    DISPATCH();

                ENTRY(MP_BC_WITH_CLEANUP): {
                    // Arriving here, there's "exception control block" on top of stack,
                    // and __exit__ bound method underneath it. Bytecode calls __exit__,
                    // and "deletes" it off stack, shifting "exception control block"
                    // to its place.
                    static const mp_obj_t no_exc[] = {mp_const_none, mp_const_none, mp_const_none};
                    if (TOP() == mp_const_none) {
                        sp--;
                        obj1 = TOP();
                        SET_TOP(mp_const_none);
                        obj2 = mp_call_function_n_kw(obj1, 3, 0, no_exc);
                    } else if (MP_OBJ_IS_SMALL_INT(TOP())) {
                        mp_obj_t cause = POP();
                        switch (MP_OBJ_SMALL_INT_VALUE(cause)) {
                            case UNWIND_RETURN: {
                                mp_obj_t retval = POP();
                                obj2 = mp_call_function_n_kw(TOP(), 3, 0, no_exc);
                                SET_TOP(retval);
                                PUSH(cause);
                                break;
                            }
                            case UNWIND_JUMP: {
                                obj2 = mp_call_function_n_kw(sp[-2], 3, 0, no_exc);
                                // Pop __exit__ boundmethod at sp[-2]
                                sp[-2] = sp[-1];
                                sp[-1] = sp[0];
                                SET_TOP(cause);
                                break;
                            }
                            default:
                                assert(0);
                        }
                    } else if (mp_obj_is_exception_type(TOP())) {
                        mp_obj_t args[3] = {sp[0], sp[-1], sp[-2]};
                        obj2 = mp_call_function_n_kw(sp[-3], 3, 0, args);
                        // Pop __exit__ boundmethod at sp[-3]
                        // TODO: Once semantics is proven, optimize for case when obj2 == True
                        sp[-3] = sp[-2];
                        sp[-2] = sp[-1];
                        sp[-1] = sp[0];
                        sp--;
                        if (mp_obj_is_true(obj2)) {
                            // This is what CPython does
                            //PUSH(MP_OBJ_NEW_SMALL_INT(UNWIND_SILENCED));
                            // But what we need to do is - pop exception from value stack...
                            sp -= 3;
                            // ... pop "with" exception handler, and signal END_FINALLY
                            // to just execute finally handler normally (by pushing None
                            // on value stack)
                            assert(exc_sp >= exc_stack);
                            assert(exc_sp->opcode == MP_BC_SETUP_WITH);
                            POP_EXC_BLOCK();
                            PUSH(mp_const_none);
                        }
                    } else {
                        assert(0);
                    }
                    DISPATCH();
                }

                ENTRY(MP_BC_UNWIND_JUMP):
                    DECODE_SLABEL;
                    PUSH((void*)(ip + unum)); // push destination ip for jump
                    PUSH((void*)(machine_uint_t)(*ip)); // push number of exception handlers to unwind
unwind_jump:
                    unum = (machine_uint_t)POP(); // get number of exception handlers to unwind
                    while (unum > 0) {
                        unum -= 1;
                        assert(exc_sp >= exc_stack);
                        if (exc_sp->opcode == MP_BC_SETUP_FINALLY || exc_sp->opcode == MP_BC_SETUP_WITH) {
                            // We're going to run "finally" code as a coroutine
                            // (not calling it recursively). Set up a sentinel
                            // on a stack so it can return back to us when it is
                            // done (when END_FINALLY reached).
                            PUSH((void*)unum); // push number of exception handlers left to unwind
                            PUSH(MP_OBJ_NEW_SMALL_INT(UNWIND_JUMP)); // push sentinel
                            ip = exc_sp->handler; // get exception handler byte code address
                            exc_sp--; // pop exception handler
                            goto dispatch_loop; // run the exception handler
                        }
                        exc_sp--;
                    }
                    ip = (const byte*)POP(); // pop destination ip for jump
                    DISPATCH();

                // matched against: POP_BLOCK or POP_EXCEPT (anything else?)
                ENTRY(MP_BC_SETUP_EXCEPT):
                ENTRY(MP_BC_SETUP_FINALLY):
                    PUSH_EXC_BLOCK();
                    DISPATCH();

                ENTRY(MP_BC_END_FINALLY):
                    // not fully implemented
                    // if TOS is an exception, reraises the exception (3 values on TOS)
                    // if TOS is None, just pops it and continues
                    // if TOS is an integer, does something else
                    // else error
                    if (mp_obj_is_exception_type(TOP())) {
                        RAISE(sp[-1]);
                    }
                    if (TOP() == mp_const_none) {
                        sp--;
                    } else if (MP_OBJ_IS_SMALL_INT(TOP())) {
                        // We finished "finally" coroutine and now dispatch back
                        // to our caller, based on TOS value
                        mp_unwind_reason_t reason = MP_OBJ_SMALL_INT_VALUE(POP());
                        switch (reason) {
                            case UNWIND_RETURN:
                                goto unwind_return;
                            case UNWIND_JUMP:
                                goto unwind_jump;
                        }
                        assert(0);
                    } else {
                        assert(0);
                    }
                    DISPATCH();

                ENTRY(MP_BC_GET_ITER):
                    SET_TOP(mp_getiter(TOP()));
                    DISPATCH();

                ENTRY(MP_BC_FOR_ITER):
                    DECODE_ULABEL; // the jump offset if iteration finishes; for labels are always forward
                    save_sp = sp;
                    assert(TOP());
                    obj1 = mp_iternext_allow_raise(TOP());
                    if (obj1 == MP_OBJ_STOP_ITERATION) {
                        --sp; // pop the exhausted iterator
                        ip += unum; // jump to after for-block
                    } else {
                        PUSH(obj1); // push the next iteration value
                    }
                    DISPATCH();

                // matched against: SETUP_EXCEPT, SETUP_FINALLY, SETUP_WITH
                ENTRY(MP_BC_POP_BLOCK):
                    // we are exiting an exception handler, so pop the last one of the exception-stack
                    assert(exc_sp >= exc_stack);
                    POP_EXC_BLOCK();
                    DISPATCH();

                // matched against: SETUP_EXCEPT
                ENTRY(MP_BC_POP_EXCEPT):
                    // TODO need to work out how blocks work etc
                    // pops block, checks it's an exception block, and restores the stack, saving the 3 exception values to local threadstate
                    assert(exc_sp >= exc_stack);
                    assert(currently_in_except_block);
                    //sp = (mp_obj_t*)(*exc_sp--);
                    //exc_sp--; // discard ip
                    POP_EXC_BLOCK();
                    //sp -= 3; // pop 3 exception values
                    DISPATCH();

                ENTRY(MP_BC_NOT):
                    if (TOP() == mp_const_true) {
                        SET_TOP(mp_const_false);
                    } else {
                        SET_TOP(mp_const_true);
                    }
                    DISPATCH();

                ENTRY(MP_BC_UNARY_OP):
                    unum = *ip++;
                    SET_TOP(mp_unary_op(unum, TOP()));
                    DISPATCH();

                ENTRY(MP_BC_BINARY_OP):
                    unum = *ip++;
                    obj2 = POP();
                    obj1 = TOP();
                    SET_TOP(mp_binary_op(unum, obj1, obj2));
                    DISPATCH();

                ENTRY(MP_BC_BUILD_TUPLE):
                    DECODE_UINT;
                    sp -= unum - 1;
                    SET_TOP(mp_obj_new_tuple(unum, sp));
                    DISPATCH();

                ENTRY(MP_BC_BUILD_LIST):
                    DECODE_UINT;
                    sp -= unum - 1;
                    SET_TOP(mp_obj_new_list(unum, sp));
                    DISPATCH();

                ENTRY(MP_BC_LIST_APPEND):
                    DECODE_UINT;
                    // I think it's guaranteed by the compiler that sp[unum] is a list
                    mp_obj_list_append(sp[-unum], sp[0]);
                    sp--;
                    DISPATCH();

                ENTRY(MP_BC_BUILD_MAP):
                    DECODE_UINT;
                    PUSH(mp_obj_new_dict(unum));
                    DISPATCH();

                ENTRY(MP_BC_STORE_MAP):
                    sp -= 2;
                    mp_obj_dict_store(sp[0], sp[2], sp[1]);
                    DISPATCH();

                ENTRY(MP_BC_MAP_ADD):
                    DECODE_UINT;
                    // I think it's guaranteed by the compiler that sp[-unum - 1] is a map
                    mp_obj_dict_store(sp[-unum - 1], sp[0], sp[-1]);
                    sp -= 2;
                    DISPATCH();

                ENTRY(MP_BC_BUILD_SET):
                    DECODE_UINT;
                    sp -= unum - 1;
                    SET_TOP(mp_obj_new_set(unum, sp));
                    DISPATCH();

                ENTRY(MP_BC_SET_ADD):
                    DECODE_UINT;
                    // I think it's guaranteed by the compiler that sp[-unum] is a set
                    mp_obj_set_store(sp[-unum], sp[0]);
                    sp--;
                    DISPATCH();

#if MICROPY_ENABLE_SLICE
                ENTRY(MP_BC_BUILD_SLICE):
                    DECODE_UINT;
                    if (unum == 2) {
                        obj2 = POP();
                        obj1 = TOP();
                        SET_TOP(mp_obj_new_slice(obj1, obj2, NULL));
                    } else {
                        obj1 = mp_obj_new_exception_msg(&mp_type_NotImplementedError, "3-argument slice is not supported");
                        nlr_pop();
                        fastn[0] = obj1;
                        return MP_VM_RETURN_EXCEPTION;
                    }
                    DISPATCH();
#endif

                ENTRY(MP_BC_UNPACK_SEQUENCE):
                    DECODE_UINT;
                    mp_unpack_sequence(sp[0], unum, sp);
                    sp += unum - 1;
                    DISPATCH();

                ENTRY(MP_BC_UNPACK_EX):
                    DECODE_UINT;
                    mp_unpack_ex(sp[0], unum, sp);
                    sp += (unum & 0xff) + ((unum >> 8) & 0xff);
                    DISPATCH();

                ENTRY(MP_BC_MAKE_FUNCTION):
                    DECODE_PTR;
                    PUSH(mp_make_function_from_raw_code((mp_raw_code_t*)unum, MP_OBJ_NULL, MP_OBJ_NULL));
                    DISPATCH();

                ENTRY(MP_BC_MAKE_FUNCTION_DEFARGS):
                    DECODE_PTR;
                    // Stack layout: def_tuple def_dict <- TOS
                    obj1 = POP();
                    SET_TOP(mp_make_function_from_raw_code((mp_raw_code_t*)unum, TOP(), obj1));
                    DISPATCH();

                ENTRY(MP_BC_MAKE_CLOSURE): {
                    DECODE_PTR;
                    machine_uint_t n_closed_over = *ip++;
                    // Stack layout: closed_overs <- TOS
                    sp -= n_closed_over - 1;
                    SET_TOP(mp_make_closure_from_raw_code((mp_raw_code_t*)unum, n_closed_over, sp));
                    DISPATCH();
                }

                ENTRY(MP_BC_MAKE_CLOSURE_DEFARGS): {
                    DECODE_PTR;
                    machine_uint_t n_closed_over = *ip++;
                    // Stack layout: def_tuple def_dict closed_overs <- TOS
                    sp -= 2 + n_closed_over - 1;
                    SET_TOP(mp_make_closure_from_raw_code((mp_raw_code_t*)unum, 0x100 | n_closed_over, sp));
                    DISPATCH();
                }

                ENTRY(MP_BC_CALL_FUNCTION):
                    DECODE_UINT;
                    // unum & 0xff == n_positional
                    // (unum >> 8) & 0xff == n_keyword
                    sp -= (unum & 0xff) + ((unum >> 7) & 0x1fe);
                    SET_TOP(mp_call_function_n_kw(*sp, unum & 0xff, (unum >> 8) & 0xff, sp + 1));
                    DISPATCH();

                ENTRY(MP_BC_CALL_FUNCTION_VAR_KW):
                    DECODE_UINT;
                    // unum & 0xff == n_positional
                    // (unum >> 8) & 0xff == n_keyword
                    // We have folowing stack layout here:
                    // fun arg0 arg1 ... kw0 val0 kw1 val1 ... seq dict <- TOS
                    sp -= (unum & 0xff) + ((unum >> 7) & 0x1fe) + 2;
                    SET_TOP(mp_call_method_n_kw_var(false, unum, sp));
                    DISPATCH();

                ENTRY(MP_BC_CALL_METHOD):
                    DECODE_UINT;
                    // unum & 0xff == n_positional
                    // (unum >> 8) & 0xff == n_keyword
                    sp -= (unum & 0xff) + ((unum >> 7) & 0x1fe) + 1;
                    SET_TOP(mp_call_method_n_kw(unum & 0xff, (unum >> 8) & 0xff, sp));
                    DISPATCH();

                ENTRY(MP_BC_CALL_METHOD_VAR_KW):
                    DECODE_UINT;
                    // unum & 0xff == n_positional
                    // (unum >> 8) & 0xff == n_keyword
                    // We have folowing stack layout here:
                    // fun self arg0 arg1 ... kw0 val0 kw1 val1 ... seq dict <- TOS
                    sp -= (unum & 0xff) + ((unum >> 7) & 0x1fe) + 3;
                    SET_TOP(mp_call_method_n_kw_var(true, unum, sp));
                    DISPATCH();

                ENTRY(MP_BC_RETURN_VALUE):
unwind_return:
                    while (exc_sp >= exc_stack) {
                        if (exc_sp->opcode == MP_BC_SETUP_FINALLY || exc_sp->opcode == MP_BC_SETUP_WITH) {
                            // We're going to run "finally" code as a coroutine
                            // (not calling it recursively). Set up a sentinel
                            // on a stack so it can return back to us when it is
                            // done (when END_FINALLY reached).
                            PUSH(MP_OBJ_NEW_SMALL_INT(UNWIND_RETURN));
                            ip = exc_sp->handler;
                            // We don't need to do anything with sp, finally is just
                            // syntactic sugar for sequential execution??
                            // sp =
                            exc_sp--;
                            goto dispatch_loop;
                        }
                        exc_sp--;
                    }
                    nlr_pop();
                    *sp_in_out = sp;
                    assert(exc_sp == exc_stack - 1);
                    return MP_VM_RETURN_NORMAL;

                ENTRY(MP_BC_RAISE_VARARGS):
                    unum = *ip++;
                    assert(unum <= 1);
                    if (unum == 0) {
                        // search for the inner-most previous exception, to reraise it
                        obj1 = MP_OBJ_NULL;
                        for (mp_exc_stack_t *e = exc_sp; e >= exc_stack; e--) {
                            if (e->prev_exc != MP_OBJ_NULL) {
                                obj1 = e->prev_exc;
                                break;
                            }
                        }
                        if (obj1 == MP_OBJ_NULL) {
                            obj1 = mp_obj_new_exception_msg(&mp_type_RuntimeError, "No active exception to reraise");
                            RAISE(obj1);
                        }
                    } else {
                        obj1 = POP();
                    }
                    obj1 = mp_make_raise_obj(obj1);
                    RAISE(obj1);

                ENTRY(MP_BC_YIELD_VALUE):
yield:
                    nlr_pop();
                    *ip_in_out = ip;
                    *sp_in_out = sp;
                    *exc_sp_in_out = MP_TAGPTR_MAKE(exc_sp, currently_in_except_block);
                    return MP_VM_RETURN_YIELD;

                ENTRY(MP_BC_YIELD_FROM): {
//#define EXC_MATCH(exc, type) MP_OBJ_IS_TYPE(exc, type)
#define EXC_MATCH(exc, type) mp_obj_exception_match(exc, type)
#define GENERATOR_EXIT_IF_NEEDED(t) if (t != MP_OBJ_NULL && EXC_MATCH(t, &mp_type_GeneratorExit)) { RAISE(t); }
                    mp_vm_return_kind_t ret_kind;
                    obj1 = POP();
                    mp_obj_t t_exc = MP_OBJ_NULL;
                    if (inject_exc != MP_OBJ_NULL) {
                        t_exc = inject_exc;
                        inject_exc = MP_OBJ_NULL;
                        ret_kind = mp_resume(TOP(), MP_OBJ_NULL, t_exc, &obj2);
                    } else {
                        ret_kind = mp_resume(TOP(), obj1, MP_OBJ_NULL, &obj2);
                    }

                    if (ret_kind == MP_VM_RETURN_YIELD) {
                        ip--;
                        PUSH(obj2);
                        goto yield;
                    }
                    if (ret_kind == MP_VM_RETURN_NORMAL) {
                        // Pop exhausted gen
                        sp--;
                        if (obj2 == MP_OBJ_NULL) {
                            // Optimize StopIteration
                            // TODO: get StopIteration's value
                            PUSH(mp_const_none);
                        } else {
                            PUSH(obj2);
                        }

                        // If we injected GeneratorExit downstream, then even
                        // if it was swallowed, we re-raise GeneratorExit
                        GENERATOR_EXIT_IF_NEEDED(t_exc);
                        DISPATCH();
                    }
                    if (ret_kind == MP_VM_RETURN_EXCEPTION) {
                        // Pop exhausted gen
                        sp--;
                        if (EXC_MATCH(obj2, &mp_type_StopIteration)) {
                            PUSH(mp_obj_exception_get_value(obj2));
                            // If we injected GeneratorExit downstream, then even
                            // if it was swallowed, we re-raise GeneratorExit
                            GENERATOR_EXIT_IF_NEEDED(t_exc);
                            DISPATCH();
                        } else {
                            RAISE(obj2);
                        }
                    }
                }

                ENTRY(MP_BC_IMPORT_NAME):
                    DECODE_QSTR;
                    obj1 = POP();
                    SET_TOP(mp_import_name(qst, obj1, TOP()));
                    DISPATCH();

                ENTRY(MP_BC_IMPORT_FROM):
                    DECODE_QSTR;
                    obj1 = mp_import_from(TOP(), qst);
                    PUSH(obj1);
                    DISPATCH();

                ENTRY(MP_BC_IMPORT_STAR):
                    mp_import_all(POP());
                    DISPATCH();

                ENTRY_DEFAULT:
                    obj1 = mp_obj_new_exception_msg(&mp_type_NotImplementedError, "byte code not implemented");
                    nlr_pop();
                    fastn[0] = obj1;
                    return MP_VM_RETURN_EXCEPTION;

#if !MICROPY_USE_COMPUTED_GOTO
                } // switch
#endif
            } // for loop

        } else {
exception_handler:
            // exception occurred

            // check if it's a StopIteration within a for block
            if (*save_ip == MP_BC_FOR_ITER && mp_obj_is_subclass_fast(mp_obj_get_type(nlr.ret_val), &mp_type_StopIteration)) {
                const byte *ip = save_ip + 1;
                machine_uint_t unum;
                DECODE_ULABEL; // the jump offset if iteration finishes; for labels are always forward
                *ip_in_out = ip + unum; // jump to after for-block
                *sp_in_out = save_sp - 1; // pop the exhausted iterator
                goto outer_dispatch_loop; // continue with dispatch loop
            }

            // set file and line number that the exception occurred at
            // TODO: don't set traceback for exceptions re-raised by END_FINALLY.
            // But consider how to handle nested exceptions.
            // TODO need a better way of not adding traceback to constant objects (right now, just GeneratorExit_obj and MemoryError_obj)
            if (mp_obj_is_exception_instance(nlr.ret_val) && nlr.ret_val != &mp_const_GeneratorExit_obj && nlr.ret_val != &mp_const_MemoryError_obj) {
                machine_uint_t code_info_size = code_info[0] | (code_info[1] << 8) | (code_info[2] << 16) | (code_info[3] << 24);
                qstr source_file = code_info[4] | (code_info[5] << 8) | (code_info[6] << 16) | (code_info[7] << 24);
                qstr block_name = code_info[8] | (code_info[9] << 8) | (code_info[10] << 16) | (code_info[11] << 24);
                machine_uint_t source_line = 1;
                machine_uint_t bc = save_ip - code_info - code_info_size;
                //printf("find %lu %d %d\n", bc, code_info[12], code_info[13]);
                for (const byte* ci = code_info + 12; *ci && bc >= ((*ci) & 31); ci++) {
                    bc -= *ci & 31;
                    source_line += *ci >> 5;
                }
                mp_obj_exception_add_traceback(nlr.ret_val, source_file, source_line, block_name);
            }

            while (currently_in_except_block) {
                // nested exception

                assert(exc_sp >= exc_stack);

                // TODO make a proper message for nested exception
                // at the moment we are just raising the very last exception (the one that caused the nested exception)

                // move up to previous exception handler
                POP_EXC_BLOCK();
            }

            if (exc_sp >= exc_stack) {
                // set flag to indicate that we are now handling an exception
                currently_in_except_block = 1;

                // catch exception and pass to byte code
                *ip_in_out = exc_sp->handler;
                mp_obj_t *sp = MP_TAGPTR_PTR(exc_sp->val_sp);
                // save this exception in the stack so it can be used in a reraise, if needed
                exc_sp->prev_exc = nlr.ret_val;
                // push(traceback, exc-val, exc-type)
                PUSH(mp_const_none);
                PUSH(nlr.ret_val);
                PUSH(mp_obj_get_type(nlr.ret_val));
                *sp_in_out = sp;

            } else {
                // propagate exception to higher level
                // TODO what to do about ip and sp? they don't really make sense at this point
                fastn[0] = nlr.ret_val; // must put exception here because sp is invalid
                return MP_VM_RETURN_EXCEPTION;
            }
        }
    }
}
