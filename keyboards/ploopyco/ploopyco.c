/* Copyright 2020 Christopher Courtney, aka Drashna Jael're  (@drashna) <drashna@live.com>
 * Copyright 2019 Sunjun Kim
 * Copyright 2020 Ploopy Corporation
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ploopyco.h"
#include "analog.h"
#include "opt_encoder.h"

#include <math.h>

// for legacy support
#if defined(OPT_DEBOUNCE) && !defined(PLOOPY_SCROLL_DEBOUNCE)
#    define PLOOPY_SCROLL_DEBOUNCE OPT_DEBOUNCE
#endif
#if defined(SCROLL_BUTT_DEBOUNCE) && !defined(PLOOPY_SCROLL_BUTTON_DEBOUNCE)
#    define PLOOPY_SCROLL_BUTTON_DEBOUNCE SCROLL_BUTT_DEBOUNCE
#endif

#ifndef PLOOPY_SCROLL_DEBOUNCE
#    define PLOOPY_SCROLL_DEBOUNCE 5
#endif
#ifndef PLOOPY_SCROLL_BUTTON_DEBOUNCE
#    define PLOOPY_SCROLL_BUTTON_DEBOUNCE 100
#endif

#ifndef PLOOPY_DPI_OPTIONS
#    define PLOOPY_DPI_OPTIONS \
         { 600, 900, 1200, 1600, 2400 }
#    ifndef PLOOPY_DPI_DEFAULT
#        define PLOOPY_DPI_DEFAULT 1
#    endif
#endif
#ifndef PLOOPY_DPI_HOLD_VALUE
#    define PLOOPY_DPI_HOLD_VALUE \
         600
#endif
#ifndef PLOOPY_DPI_DEFAULT
#    define PLOOPY_DPI_DEFAULT 0
#endif
#ifndef PLOOPY_DRAGSCROLL_DIVISOR_H
#    define PLOOPY_DRAGSCROLL_DIVISOR_H 8.0
#endif
#ifndef PLOOPY_DRAGSCROLL_DIVISOR_V
#    define PLOOPY_DRAGSCROLL_DIVISOR_V 8.0
#endif
#ifndef ENCODER_BUTTON_ROW
#    define ENCODER_BUTTON_ROW 0
#endif
#ifndef ENCODER_BUTTON_COL
#    define ENCODER_BUTTON_COL 0
#endif

struct Vec2 {
   float x;
   float y;
};
struct Vec2 previous_pvector = {0.f, 0.f};
struct Vec2 previous_pnormal_left = {0.f, 0.f};
struct Vec2 previous_pnormal_right = {0.f, 0.f};
bool previous_rotation_ccw = true;
float v2_length(struct Vec2* v) {
    return sqrtf(v->x*v->x + v->y*v->y);
}
struct Vec2 v2_scalar_div(struct Vec2* v, float s) {
    return (struct Vec2){v->x / s, v->y / s};
}
struct Vec2 v2_scalar_mul(struct Vec2* v, float s) {
    return (struct Vec2){v->x * s, v->y * s};
}
struct Vec2 v2_normalize(struct Vec2* p) {
    if (p == NULL) {
        return (struct Vec2){0.f, 0.f};
    }

    float l = v2_length(p);
    return v2_scalar_div(p, l);
}
float v2_dot(struct Vec2* l, struct Vec2* r) {
    return l->x * r-> x + l->y * r->y;
}
struct Vec2 v2_add(struct Vec2*l, struct Vec2* r) {
    return (struct Vec2){l->x + r->x, l->y + r->y};
}
struct Vec2 v2_sub(struct Vec2*l, struct Vec2* r) {
    return (struct Vec2){l->x - r->x, l->y - r->y};
}
float v2_cos_theta(struct Vec2* l, struct Vec2* r) {
    struct Vec2 nl = v2_normalize(l);
    struct Vec2 nr = v2_normalize(r);

    return v2_dot(&nl, &nr);
}

keyboard_config_t keyboard_config;
uint16_t          dpi_array[] = PLOOPY_DPI_OPTIONS;
#define DPI_OPTION_SIZE ARRAY_SIZE(dpi_array)
uint16_t          dpi_hold    = PLOOPY_DPI_HOLD_VALUE;

// Trackball State
bool  is_scroll_clicked    = false;
bool  is_drag_scroll       = false;
float scroll_accumulated_h = 0;
float scroll_accumulated_v = 0;
bool  is_dpi_held          = false;
float flip_accumulated     = 0;
bool  is_tab_flip          = false;

#ifdef ENCODER_ENABLE
uint16_t lastScroll        = 0; // Previous confirmed wheel event
uint16_t lastMidClick      = 0; // Stops scrollwheel from being read if it was pressed
pin_t    encoder_pins_a[1] = ENCODER_A_PINS;
pin_t    encoder_pins_b[1] = ENCODER_B_PINS;
bool     debug_encoder     = false;

bool encoder_update_kb(uint8_t index, bool clockwise) {
    if (!encoder_update_user(index, clockwise)) {
        return false;
    }
#    ifdef MOUSEKEY_ENABLE
    tap_code(clockwise ? MS_WHLU : MS_WHLD);
#    else
    report_mouse_t mouse_report = pointing_device_get_report();
    mouse_report.v              = clockwise ? 1 : -1;
    pointing_device_set_report(mouse_report);
    pointing_device_send();
#    endif
    return true;
}

void encoder_driver_init(void) {
    for (uint8_t i = 0; i < ARRAY_SIZE(encoder_pins_a); i++) {
        gpio_set_pin_input(encoder_pins_a[i]);
        gpio_set_pin_input(encoder_pins_b[i]);
    }
    opt_encoder_init();
}

void encoder_driver_task(void) {
    uint16_t p1 = analogReadPin(encoder_pins_a[0]);
    uint16_t p2 = analogReadPin(encoder_pins_b[0]);

    if (debug_encoder) dprintf("OPT1: %d, OPT2: %d\n", p1, p2);

    int8_t dir = opt_encoder_handler(p1, p2);
    // If the mouse wheel was just released, do not scroll.
    if (timer_elapsed(lastMidClick) < PLOOPY_SCROLL_BUTTON_DEBOUNCE) {
        return;
    }

    // Limit the number of scrolls per unit time.
    if (timer_elapsed(lastScroll) < PLOOPY_SCROLL_DEBOUNCE) {
        return;
    }

    // Don't scroll if the middle button is depressed.
    if (is_scroll_clicked) {
#    ifndef PLOOPY_IGNORE_SCROLL_CLICK
        return;
#    endif
    }

    if (dir == 0) return;
    encoder_queue_event(0, dir > 0);
    lastScroll = timer_read();
}
#endif

void toggle_drag_scroll(void) {
    // is_drag_scroll ^= 1;
}

void cycle_dpi(void) {
    keyboard_config.dpi_config = (keyboard_config.dpi_config + 1) % DPI_OPTION_SIZE;
    eeconfig_update_kb(keyboard_config.raw);
    pointing_device_set_cpi(dpi_array[keyboard_config.dpi_config]);
}

float get_pointer_rotation(int8_t x, int8_t y) {
    float result = 0.f;

    struct Vec2 newP = {(float)x, (float)y};
    newP = v2_add(&newP, &previous_pvector);
    // struct PointerVector newLeft = {-newP.y, newP.x};
    // struct PointerVector newRight = {newP.y, -newP.x};
    struct Vec2 newLeft = {-y, x};
    newLeft = v2_add(&newP, &newLeft);
    struct Vec2 newRight = {y, -x};
    newRight = v2_add(&newP, &newRight);

    // get the length of newRight - prevRight and newLeft - prevLeft
    // if lengthL < lengthR then this is a CCW rotation
    // if they are equal then use the previous rotation direction
    bool ccw = previous_rotation_ccw;
    struct Vec2 left_delta = v2_sub(&newLeft, &previous_pnormal_left);
    struct Vec2 right_delta = v2_sub(&newRight, &previous_pnormal_right);
    float left_length = v2_length(&left_delta);
    float right_length = v2_length(&right_delta);
    if (left_length < right_length) {
        ccw = true;
    } else if (left_length > right_length) {
        ccw = false;
    }

    // float cos_theta = abs(v2_cos_theta(&previous_pvector, &newP));

    previous_rotation_ccw = ccw;
    previous_pvector = newP;
    previous_pnormal_left = newLeft;
    previous_pnormal_right = newRight;

    return result;
}

report_mouse_t pointing_device_task_kb(report_mouse_t mouse_report) {
    mouse_report = pointing_device_task_user(mouse_report);
    if (is_drag_scroll) {
        scroll_accumulated_h += (float)mouse_report.x / PLOOPY_DRAGSCROLL_DIVISOR_H;
        scroll_accumulated_v += (float)mouse_report.y / PLOOPY_DRAGSCROLL_DIVISOR_V;

        // Assign integer parts of accumulated scroll values to the mouse report
        mouse_report.h = (int8_t)scroll_accumulated_h;
#ifdef PLOOPY_DRAGSCROLL_INVERT
        mouse_report.v = -(int8_t)scroll_accumulated_v;
#else
        mouse_report.v = (int8_t)scroll_accumulated_v;
#endif

        // Update accumulated scroll values by subtracting the integer parts
        scroll_accumulated_h -= (int8_t)scroll_accumulated_h;
        scroll_accumulated_v -= (int8_t)scroll_accumulated_v;

        // Clear the X and Y values of the mouse report
        mouse_report.x = 0;
        mouse_report.y = 0;
    } else if (is_tab_flip) {
        flip_accumulated += get_pointer_rotation(mouse_report.x, mouse_report.y);

        mouse_report.x = 0;
        mouse_report.y = 0;
    }

    return mouse_report;
}

bool process_record_kb(uint16_t keycode, keyrecord_t* record) {
    if (debug_mouse) {
        dprintf("KL: kc: %u, col: %u, row: %u, pressed: %u\n", keycode, record->event.key.col, record->event.key.row, record->event.pressed);
    }

    // Update Timer to prevent accidental scrolls
#ifdef ENCODER_ENABLE
    if ((record->event.key.col == ENCODER_BUTTON_COL) && (record->event.key.row == ENCODER_BUTTON_ROW)) {
        lastMidClick      = timer_read();
        is_scroll_clicked = record->event.pressed;
    }
#endif

    if (!process_record_user(keycode, record)) {
        return false;
    }

    if (keycode == DPI_CONFIG && record->event.pressed) {
        cycle_dpi();
    }

    if (keycode == DPI_HOLD) {
        if (record->event.pressed) {
            if (!is_dpi_held) {
                is_dpi_held = true;
                pointing_device_set_cpi(dpi_hold);
            }
        } else {
            is_dpi_held = false;
            pointing_device_set_cpi(dpi_array[keyboard_config.dpi_config]);
        }
    }

    if (keycode == DRAG_SCROLL) {
// #ifdef PLOOPY_DRAGSCROLL_MOMENTARY
        is_drag_scroll = record->event.pressed;
// #else
//         if (record->event.pressed) {
//             toggle_drag_scroll();
//         }
// #endif
    }

    if (keycode == TAB_FLIP) {
        if (record->event.pressed) {
            previous_rotation_ccw = true;
            previous_pvector = (struct Vec2){0.f, 0.f};
            previous_pnormal_left = (struct Vec2){0.f, 0.f};
            previous_pnormal_right = (struct Vec2){0.f, 0.f};
            is_tab_flip = true;
        }
    }

    return true;
}

// Hardware Setup
void keyboard_pre_init_kb(void) {
    // debug_enable  = true;
    // debug_matrix  = true;
    // debug_mouse   = true;
    // debug_encoder = true;

    /* Ground all output pins connected to ground. This provides additional
     * pathways to ground. If you're messing with this, know this: driving ANY
     * of these pins high will cause a short. On the MCU. Ka-blooey.
     */
#ifdef UNUSABLE_PINS
    const pin_t unused_pins[] = UNUSABLE_PINS;

    for (uint8_t i = 0; i < ARRAY_SIZE(unused_pins); i++) {
        gpio_set_pin_output_push_pull(unused_pins[i]);
        gpio_write_pin_low(unused_pins[i]);
    }
#endif

    // This is the debug LED.
#if defined(DEBUG_LED_PIN)
    gpio_set_pin_output_push_pull(DEBUG_LED_PIN);
    gpio_write_pin(DEBUG_LED_PIN, debug_enable);
#endif

    keyboard_pre_init_user();
}

void pointing_device_init_kb(void) {
    keyboard_config.raw = eeconfig_read_kb();
    if (keyboard_config.dpi_config > DPI_OPTION_SIZE) {
        eeconfig_init_kb();
    }
    pointing_device_set_cpi(dpi_array[keyboard_config.dpi_config]);
}

void eeconfig_init_kb(void) {
    keyboard_config.dpi_config = PLOOPY_DPI_DEFAULT;
    eeconfig_update_kb(keyboard_config.raw);
    eeconfig_init_user();
}
