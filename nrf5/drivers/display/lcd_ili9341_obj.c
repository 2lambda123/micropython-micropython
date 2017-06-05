/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Glenn Ruben Bakke
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

#include "py/obj.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "genhdr/pins.h"

#include "lcd_ili9341_driver.h"
// For now PWM is only enabled for nrf52 targets.
#if MICROPY_PY_DISPLAY_LCD_ILI9341

/// \moduleref display
/// \class ILI9341 - ILI9341 TFT LCD display driver.

#include "moddisplay.h"
#include "framebuffer.h"
#include "pin.h"
#include "spi.h"

typedef struct _lcd_ili9341_obj_t {
    mp_obj_base_t base;
    display_draw_callbacks_t draw_callbacks;
    framebuffer_t * framebuffer;
    machine_hard_spi_obj_t *spi;
    pin_obj_t * pin_cs;
    pin_obj_t * pin_dc;
} lcd_ili9341_obj_t;

#define LCD_ILI9341_COLOR_BLACK 0
#define LCD_ILI9341_COLOR_WHITE 1

static void set_pixel(void * p_display,
                      uint16_t    x,
                      uint16_t    y,
                      uint16_t    color) {
    lcd_ili9341_obj_t *self = (lcd_ili9341_obj_t *)p_display;

    if (color == LCD_ILI9341_COLOR_BLACK) {
        framebuffer_pixel_clear(self->framebuffer, x, y);
    } else {
        framebuffer_pixel_set(self->framebuffer, x, y);
    }
}

/// \method __str__()
/// Return a string describing the ILI9341 object.
STATIC void lcd_ili9341_print(const mp_print_t *print, mp_obj_t o, mp_print_kind_t kind) {
    lcd_ili9341_obj_t *self = o;

    mp_printf(print, "ILI9341(SPI(mosi=(port=%u, pin=%u), miso=(port=%u, pin=%u), clk=(port=%u, pin=%u)),\n",
                     self->spi->pyb->spi->init.mosi_pin_port,
                     self->spi->pyb->spi->init.mosi_pin,
                     self->spi->pyb->spi->init.miso_pin_port,
                     self->spi->pyb->spi->init.miso_pin,
                     self->spi->pyb->spi->init.clk_pin_port,
                     self->spi->pyb->spi->init.clk_pin
                     );

    mp_printf(print, "        cs=(port=%u, pin=%u), dc=(port=%u, pin=%u),\n",
                     self->pin_cs->port,
                     self->pin_cs->pin,
                     self->pin_dc->port,
                     self->pin_dc->pin);

    mp_printf(print, "        FB(width=%u, height=%u, dir=%u, fb_stride=%u, fb_dirty_stride=%u))\n",
                     self->framebuffer->screen_width,
                     self->framebuffer->screen_height,
                     self->framebuffer->line_orientation,
                     self->framebuffer->fb_stride,
                     self->framebuffer->fb_dirty_stride);
}

// for make_new
enum {
    ARG_NEW_WIDTH,
    ARG_NEW_HEIGHT,
    ARG_NEW_SPI,
    ARG_NEW_CS,
    ARG_NEW_DC,
};

/*

Example for nrf51822 / pca10028:

from machine import Pin, SPI
from display import ILI9341
cs = Pin("A17", mode=Pin.OUT, pull=Pin.PULL_UP)
dc = Pin("A18", mode=Pin.OUT, pull=Pin.PULL_UP)
spi = SPI(0, baudrate=8000000)
d = ILI9341(240, 320, spi, cs, dc)
d.text("Hello World!", 32, 32)
d.show()

Example for nrf52832 / pca10040:

from machine import Pin, SPI
from display import ILI9341
cs = Pin("A16", mode=Pin.OUT, pull=Pin.PULL_UP)
dc = Pin("A17", mode=Pin.OUT, pull=Pin.PULL_UP)
spi = SPI(0, baudrate=8000000)
d = ILI9341(240, 320, spi, cs, dc)
d.text("Hello World!", 32, 32)
d.show()

Example for nrf52840 / pca10056:

from machine import Pin, SPI
from display import ILI9341
cs = Pin("B6", mode=Pin.OUT, pull=Pin.PULL_UP)
dc = Pin("B7", mode=Pin.OUT, pull=Pin.PULL_UP)
spi = SPI(0, baudrate=8000000)
d = ILI9341(240, 320, spi, cs, dc)
d.text("Hello World!", 32, 32)
d.show()

*/
STATIC mp_obj_t lcd_ili9341_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    static const mp_arg_t allowed_args[] = {
        { ARG_NEW_WIDTH,  MP_ARG_REQUIRED | MP_ARG_INT },
        { ARG_NEW_HEIGHT, MP_ARG_REQUIRED | MP_ARG_INT },
        { ARG_NEW_SPI,    MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { ARG_NEW_CS,     MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { ARG_NEW_DC,     MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    lcd_ili9341_obj_t *s = m_new_obj_with_finaliser(lcd_ili9341_obj_t);
    s->base.type = type;
    s->draw_callbacks.pixel_set = set_pixel;

    mp_int_t width;
    mp_int_t height;

    if (args[ARG_NEW_WIDTH].u_int > 0) {
        width = args[ARG_NEW_WIDTH].u_int;
    } else {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError,
                  "Display width not set"));
    }

    if (args[ARG_NEW_HEIGHT].u_int > 0) {
        height = args[ARG_NEW_HEIGHT].u_int;
    } else {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError,
                  "Display height not set"));
    }

    if (args[ARG_NEW_SPI].u_obj != MP_OBJ_NULL) {
        s->spi = args[ARG_NEW_SPI].u_obj;
    } else {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError,
                  "Display SPI not set"));
    }

    if (args[ARG_NEW_CS].u_obj != MP_OBJ_NULL) {
        s->pin_cs = args[ARG_NEW_CS].u_obj;
    } else {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError,
                  "Display CS Pin not set"));
    }

    if (args[ARG_NEW_DC].u_obj != MP_OBJ_NULL) {
        s->pin_dc = args[ARG_NEW_DC].u_obj;
    } else {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError,
                  "Display DC Pin not set"));
    }

    framebuffer_init_t init_conf = {
        .width = width,
        .height = height,
        .line_orientation = FRAMEBUFFER_LINE_DIR_HORIZONTAL,
        .double_buffer = false
    };

    s->framebuffer = m_new(framebuffer_t, sizeof(framebuffer_t));

    framebuffer_init(s->framebuffer, &init_conf);

    driver_ili9341_init(s->spi->pyb->spi->instance, s->pin_cs, s->pin_dc);
    // Default to white background
    driver_ili9341_clear(0x0000);

    framebuffer_clear(s->framebuffer);

    return MP_OBJ_FROM_PTR(s);
}

// text

/// \method fill(color)
/// Fill framebuffer with the color defined as argument.
STATIC mp_obj_t lcd_ili9341_fill(mp_obj_t self_in, mp_obj_t color) {
    lcd_ili9341_obj_t *self = MP_OBJ_TO_PTR(self_in);

    if (color == MP_OBJ_NEW_SMALL_INT(LCD_ILI9341_COLOR_BLACK)) {
        framebuffer_clear(self->framebuffer);
    } else {
        framebuffer_fill(self->framebuffer);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lcd_ili9341_fill_obj, lcd_ili9341_fill);

static void render(framebuffer_t * p_framebuffer) {
    for (uint16_t i = 0; i < p_framebuffer->fb_dirty_stride; i++) {
        if (p_framebuffer->fb_dirty[i].byte != 0) {
            for (uint16_t b = 0; b < 8; b++) {
                if ((((p_framebuffer->fb_dirty[i].byte >> b) & 0x01) == 1)) {
                    uint16_t line_num = (i * 8) + b;
                    driver_ili9341_update_line(line_num,
                        &p_framebuffer->fb_new[line_num * p_framebuffer->fb_stride],
						p_framebuffer->fb_stride);
                }
            }

            p_framebuffer->fb_dirty[i].byte = 0x00;
        }
    }
}

/// \method show()
/// Display content in framebuffer.
STATIC mp_obj_t lcd_ili9341_show(size_t n_args, const mp_obj_t *args) {
    lcd_ili9341_obj_t *self = MP_OBJ_TO_PTR(args[0]);

    render(self->framebuffer);
    framebuffer_flip(self->framebuffer);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lcd_ili9341_show_obj, 1, 2, lcd_ili9341_show);

/// \method refresh([num_of_refresh])
/// Refresh content in framebuffer.
///
///   - With no argument, 1 refresh will be done.
///   - With `num_of_refresh` given, The whole framebuffer will be considered
///   dirty and will be refreshed the given number of times.
STATIC mp_obj_t lcd_ili9341_refresh(mp_obj_t self_in) {
    lcd_ili9341_obj_t *self = MP_OBJ_TO_PTR(self_in);

    (void)self;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lcd_ili9341_refresh_obj, lcd_ili9341_refresh);

/// \method pixel(x, y, [color])
/// Write one pixel in framebuffer.
///
///   - With no argument, the color of the pixel in framebuffer will be returend.
///   - With `color` given, sets the pixel to the color given.
STATIC mp_obj_t lcd_ili9341_pixel(size_t n_args, const mp_obj_t *args) {
    lcd_ili9341_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    mp_int_t x = mp_obj_get_int(args[1]);
    mp_int_t y = mp_obj_get_int(args[2]);
    mp_int_t color = mp_obj_get_int(args[3]);

    set_pixel(self, x, y, color);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lcd_ili9341_pixel_obj, 3, 4, lcd_ili9341_pixel);

/// \method pixel(text, x, y, [color])
/// Write one pixel in framebuffer.
///
///   - With no argument, the color will be the opposite of background (fill color).
///   - With `color` given, sets the pixel to the color given.
STATIC mp_obj_t lcd_ili9341_text(size_t n_args, const mp_obj_t *args) {
    lcd_ili9341_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    const char *str = mp_obj_str_get_str(args[1]);
    mp_int_t x = mp_obj_get_int(args[2]);
    mp_int_t y = mp_obj_get_int(args[3]);
    mp_int_t color;
    if (n_args >= 4) {
        color = mp_obj_get_int(args[3]);
    }

    // display_print_string(self->framebuffer, x, y, str);

    (void)x;
    (void)y;
    (void)self;
    (void)str;
    (void)color;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lcd_ili9341_text_obj, 4, 5, lcd_ili9341_text);

STATIC mp_obj_t lcd_ili9341_del(mp_obj_t self_in) {
    lcd_ili9341_obj_t *self = MP_OBJ_TO_PTR(self_in);

    (void)self;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lcd_ili9341_del_obj, lcd_ili9341_del);

STATIC const mp_map_elem_t lcd_ili9341_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___del__), (mp_obj_t)(&lcd_ili9341_del_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_fill), (mp_obj_t)(&lcd_ili9341_fill_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_show), (mp_obj_t)(&lcd_ili9341_show_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_text), (mp_obj_t)(&lcd_ili9341_text_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_pixel), (mp_obj_t)(&lcd_ili9341_pixel_obj) },
#if 0
    { MP_OBJ_NEW_QSTR(MP_QSTR_bitmap), (mp_obj_t)(&lcd_ili9341_bitmap_obj) },
#endif
    { MP_OBJ_NEW_QSTR(MP_QSTR_COLOR_BLACK), MP_OBJ_NEW_SMALL_INT(LCD_ILI9341_COLOR_BLACK) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_COLOR_WHITE), MP_OBJ_NEW_SMALL_INT(LCD_ILI9341_COLOR_WHITE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_VERTICAL), MP_OBJ_NEW_SMALL_INT(0) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_HORIZONTAL), MP_OBJ_NEW_SMALL_INT(1) },
};

STATIC MP_DEFINE_CONST_DICT(lcd_ili9341_locals_dict, lcd_ili9341_locals_dict_table);

const mp_obj_type_t lcd_ili9341_type = {
    { &mp_type_type },
    .name = MP_QSTR_ILI9341,
    .print = lcd_ili9341_print,
    .make_new = lcd_ili9341_make_new,
    .locals_dict = (mp_obj_t)&lcd_ili9341_locals_dict
};

#endif // MICROPY_PY_DISPLAY_LCD_ILI9341
