#include "modmodbus.h"

#include "py/objarray.h"

#include "esp_log.h"
#include "uart.h"

#include <string.h>

#if ESP_IDF_VERSION_MINOR >= 3

bool slave_set = false;
bool master_set = false;

char area_names[4][9] = {"HOLDING", "INPUT", "COIL", "DISCRETE"};
char parity_names[4][6] = {"NONE", "ERROR", "EVEN", "ODD"};
char serial_mode_names[2][5] = {"RTU", "ASCII"};
char data_type_names[5][6] = {"U8", "U16", "U32", "FLOAT", "ASCII"};

int cid_counter = 0;

mp_obj_t param_descriptions = NULL;


/****************************
 *
 *     Serial Master
 *
 * **************************
 */


/* modbus.Serial_Master(
    uart_port=int,
    baudrate=int,
    parity=PARITY_NONE, PARITY_EVEN, PARITY_ODD,
    tx=int,
    rx=int,
    rts=int,
    serial_mode=SERIAL_MODE_RTU, SERIAL_MODE_ASCII
) */
STATIC mp_obj_t modbus_serial_master_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *in_args) {
    mp_arg_check_num(n_args, n_kw, 2, 7, true);

    enum { ARG_uart_port, ARG_baudrate, ARG_parity, ARG_tx, ARG_rx, ARG_rts, ARG_serial_mode };

    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_uart_port, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0}},
        { MP_QSTR_baudrate, MP_ARG_INT, {.u_int = 9600}},
        { MP_QSTR_parity, MP_ARG_INT, {.u_int = UART_PARITY_DISABLE}},
        { MP_QSTR_tx, MP_ARG_INT, {.u_int = 12}},
        { MP_QSTR_rx, MP_ARG_INT, {.u_int = 13}},
        { MP_QSTR_rts, MP_ARG_INT, {.u_int = 32}},
        { MP_QSTR_serial_mode, MP_ARG_INT, {.u_int = MB_MODE_RTU}},
    };

    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, in_args + n_args);

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, in_args, &kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if ((args[ARG_uart_port].u_int < 0) || (args[ARG_uart_port].u_int > UART_NUM_MAX)) {
        mp_raise_ValueError(MP_ERROR_TEXT("Uart Port not allowed"));
    }

    if ((args[ARG_uart_port].u_int < 0) || (args[ARG_uart_port].u_int > 3)) {
        mp_raise_ValueError(MP_ERROR_TEXT("Parity must be PARITY_NONE, _EVEN or _ODD"));
    }

    if ((args[ARG_tx].u_int < 0) || (args[ARG_tx].u_int > 34)) {
        mp_raise_ValueError(MP_ERROR_TEXT("TX-Pin not allowed"));
    }

    if ((args[ARG_rx].u_int < 0) || (args[ARG_rx].u_int > 34)) {
        mp_raise_ValueError(MP_ERROR_TEXT("RX-Pin not allowed"));
    }

    if ((args[ARG_rts].u_int < 0) || (args[ARG_rts].u_int > 34)) {
        mp_raise_ValueError(MP_ERROR_TEXT("RTS-Pin not allowed"));
    }

    if ((args[ARG_serial_mode].u_int < 0) || (args[ARG_serial_mode].u_int > 1)) {
        mp_raise_ValueError(MP_ERROR_TEXT("Serial Mode must be SERIAL_MODE_RTU or SERIAL_MODE_ASCII"));
    }

    if (master_set) {
        mp_raise_ValueError(MP_ERROR_TEXT("Serial Master already initialized. Only one is allowed."));
    }

    modbus_serial_master_obj_t *self = m_new_obj(modbus_serial_master_obj_t);
    self->base.type = &modbus_serial_master_type;
    self->uart_port = args[ARG_uart_port].u_int;
    self->baudrate = args[ARG_baudrate].u_int;
    self->parity = args[ARG_parity].u_int;
    self->tx = args[ARG_tx].u_int;
    self->rx = args[ARG_rx].u_int;
    self->rts = args[ARG_rts].u_int;
    self->serial_mode = args[ARG_serial_mode].u_int;

    void *mbc_master_handler = NULL;
    ESP_ERROR_CHECK(mbc_master_init(MB_PORT_SERIAL_MASTER, &mbc_master_handler));
    master_set = true;

    return MP_OBJ_FROM_PTR(self);
};


STATIC void modbus_serial_master_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    modbus_serial_master_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "Serial_Master(uart_port=%u, baudrate=%u, parity=%s, tx=%u, rx=%u, rts=%u, serial_mode=%s)",
        self->uart_port, self->baudrate, parity_names[self->parity], self->tx, self->rx, self->rts, serial_mode_names[self->serial_mode]);
};


STATIC mp_obj_t modbus_serial_master_run(mp_obj_t self_in) {
    modbus_serial_master_obj_t *self = self_in;
    mb_communication_info_t comm_info;
    comm_info.mode = self->serial_mode;
    comm_info.port = self->uart_port;
    comm_info.baudrate = self->baudrate;
    comm_info.parity = self->parity;
    ESP_ERROR_CHECK(mbc_master_setup((void *)&comm_info));

    ESP_ERROR_CHECK(uart_set_pin(self->uart_port, self->tx,
        self->rx, self->rts, UART_PIN_NO_CHANGE));

    ESP_ERROR_CHECK(mbc_master_start());

    ESP_ERROR_CHECK(uart_set_mode(self->uart_port, UART_MODE_RS485_HALF_DUPLEX));

    mp_obj_list_t *list = (mp_obj_list_t *)param_descriptions;
    mb_parameter_descriptor_t *device_parameters;
    device_parameters = calloc(list->len, sizeof(mb_parameter_descriptor_t));
    for (uint8_t i = 0; i < list->len; i++) {
        modbus_parameter_descriptor_obj_t *element = (modbus_parameter_descriptor_obj_t *)list->items[i];
        device_parameters[i].cid = element->cid;
        device_parameters[i].param_key = mp_obj_str_get_str(element->param_key);
        device_parameters[i].param_units = mp_obj_str_get_str(element->param_unit);
        device_parameters[i].mb_slave_addr = element->mb_slave_addr;
        device_parameters[i].mb_param_type = element->mb_param_type;
        device_parameters[i].mb_reg_start = element->mb_reg_start;
        device_parameters[i].mb_size = element->mb_size;
        device_parameters[i].param_offset = element->param_offset;
        device_parameters[i].param_type = element->param_type;
        device_parameters[i].param_size = element->param_size;
        device_parameters[i].access = element->access;
    }
    vTaskDelay(5);
    ESP_ERROR_CHECK(mbc_master_set_descriptor(&device_parameters[0], list->len));

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(modbus_serial_master_run_obj, modbus_serial_master_run);


STATIC void modbus_serial_master_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    modbus_serial_master_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (dest[0] == MP_OBJ_NULL) {
        // Load
        if (attr == MP_QSTR_uart_port) {
            dest[0] = mp_obj_new_int(self->uart_port);
        } else if (attr == MP_QSTR_baudrate) {
            dest[0] = mp_obj_new_int(self->baudrate);
        } else if (attr == MP_QSTR_parity) {
            dest[0] = mp_obj_new_int(self->parity);
        } else if (attr == MP_QSTR_tx) {
            dest[0] = mp_obj_new_int(self->tx);
        } else if (attr == MP_QSTR_rx) {
            dest[0] = mp_obj_new_int(self->rx);
        } else if (attr == MP_QSTR_rts) {
            dest[0] = mp_obj_new_int(self->rts);
        } else if (attr == MP_QSTR_serial_mode) {
            dest[0] = mp_obj_new_int(self->serial_mode);
        } else if (attr == MP_QSTR_run) {
            dest[0] = MP_OBJ_FROM_PTR(&modbus_serial_master_run_obj);
            dest[1] = self_in;
        }
    } else if (dest[1] != MP_OBJ_NULL) {
        // Store
        mp_raise_ValueError(MP_ERROR_TEXT("Parameter is Read Only"));
    }
}

/****************************
 *
 *     TCP Master
 *
 * **************************
 */

/* modbus.TCP_Master(
    ip=str,
    port=int,
    interface=nw_interface
) */
STATIC mp_obj_t modbus_tcp_master_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *in_args) {
    mp_arg_check_num(n_args, n_kw, 3, 3, true);

    enum { ARG_ip, ARG_port, ARG_interface };

    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_ip, MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_obj = mp_const_none}},
        { MP_QSTR_port, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0}},
        { MP_QSTR_interface, MP_ARG_OBJ, {.u_obj = mp_const_none}},
    };

    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, in_args + n_args);

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, in_args, &kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (args[ARG_ip].u_obj == mp_const_none) {
        mp_raise_ValueError(MP_ERROR_TEXT("IP cannot be None"));
    }

    if ((args[ARG_port].u_int < 1) || (args[ARG_port].u_int > 65535)) {
        mp_raise_ValueError(MP_ERROR_TEXT("Port must be in 1..65535"));
    }

    if (args[ARG_interface].u_obj == mp_const_none) {
        mp_raise_ValueError(MP_ERROR_TEXT("Interface cannot be None"));
    }

    if (master_set) {
        mp_raise_ValueError(MP_ERROR_TEXT("TCP Master already initialized. Only one is allowed."));
    }

    modbus_tcp_master_obj_t *self = m_new_obj(modbus_tcp_master_obj_t);
    self->base.type = &modbus_tcp_master_type;
    self->ip4 = args[ARG_ip].u_obj;
    self->port = args[ARG_port].u_int;
    self->network_if = args[ARG_interface].u_obj;

    void *mbc_master_handler = NULL;
    ESP_ERROR_CHECK(mbc_master_init_tcp(&mbc_master_handler));
    master_set = true;

    return MP_OBJ_FROM_PTR(self);
};



STATIC void modbus_tcp_master_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    modbus_tcp_master_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "TCP_Master(port=%u)",
        self->port);
};


STATIC mp_obj_t modbus_tcp_master_run(mp_obj_t self_in) {
    modbus_tcp_master_obj_t *self = self_in;
    mb_communication_info_t comm_info;
    comm_info.ip_mode = MB_MODE_TCP;
    comm_info.ip_addr_type = MB_IPV4;
    comm_info.ip_port = self->port;
    char *slave_ip_address_table[2] = {
        NULL,
        NULL
    };

    slave_ip_address_table[0] = mp_obj_str_get_str(self->ip4);

    comm_info.ip_addr = slave_ip_address_table;

    if (mp_obj_get_type(self->network_if) == &wifi_network_if_type) {
        wifi_network_if_obj_t *interface = self->network_if;
        comm_info.ip_netif_ptr = interface->netif;
    }
    if (mp_obj_get_type(self->network_if) == &eth_network_if_type) {
        eth_network_if_obj_t *interface = self->network_if;
        comm_info.ip_netif_ptr = interface->netif;
    }
    ESP_ERROR_CHECK(mbc_master_setup((void *)&comm_info));

    mp_obj_list_t *list = (mp_obj_list_t *)param_descriptions;
    mb_parameter_descriptor_t *device_parameters;
    device_parameters = calloc(list->len, sizeof(mb_parameter_descriptor_t));
    for (uint8_t i = 0; i < list->len; i++) {
        modbus_parameter_descriptor_obj_t *element = (modbus_parameter_descriptor_obj_t *)list->items[i];
        device_parameters[i].cid = element->cid;
        device_parameters[i].param_key = mp_obj_str_get_str(element->param_key);
        device_parameters[i].param_units = mp_obj_str_get_str(element->param_unit);
        device_parameters[i].mb_slave_addr = element->mb_slave_addr;
        device_parameters[i].mb_param_type = element->mb_param_type;
        device_parameters[i].mb_reg_start = element->mb_reg_start;
        device_parameters[i].mb_size = element->mb_size;
        device_parameters[i].param_offset = element->param_offset;
        device_parameters[i].param_type = element->param_type;
        device_parameters[i].param_size = element->param_size;
        device_parameters[i].access = element->access;
    }

    ESP_ERROR_CHECK(mbc_master_set_descriptor(&device_parameters[0], list->len));

    mbc_master_start();
    vTaskDelay(5);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(modbus_tcp_master_run_obj, modbus_tcp_master_run);


STATIC void modbus_tcp_master_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    modbus_tcp_master_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (dest[0] == MP_OBJ_NULL) {
        // Load
        if (attr == MP_QSTR_port) {
            dest[0] = mp_obj_new_int(self->port);
        } else if (attr == MP_QSTR_ip) {
            dest[0] = self->ip4;
        } else if (attr == MP_QSTR_run) {
            dest[0] = MP_OBJ_FROM_PTR(&modbus_tcp_master_run_obj);
            dest[1] = self_in;
        }
    } else if (dest[1] != MP_OBJ_NULL) {
        // Store
        mp_raise_ValueError(MP_ERROR_TEXT("Parameter is Read Only"));
    }
}

const mp_obj_type_t modbus_serial_master_type = {
    { &mp_type_type },
    .name = MP_QSTR_Serial_Master,
    .print = modbus_serial_master_print,
    .make_new = modbus_serial_master_make_new,
    .attr = modbus_serial_master_attr,
};

const mp_obj_type_t modbus_tcp_master_type = {
    { &mp_type_type },
    .name = MP_QSTR_TCP_Master,
    .print = modbus_tcp_master_print,
    .make_new = modbus_tcp_master_make_new,
    .attr = modbus_tcp_master_attr,
};
#endif
