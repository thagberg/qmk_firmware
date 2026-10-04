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
#include "vec2.h"

#include <math.h>
#include <stdlib.h>

#include "print.h"

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

keyboard_config_t keyboard_config;
uint16_t          dpi_array[] = PLOOPY_DPI_OPTIONS;
#define DPI_OPTION_SIZE ARRAY_SIZE(dpi_array)
uint16_t          dpi_hold    = PLOOPY_DPI_HOLD_VALUE;

#define MAX_CACHED_DELTAS 5
struct DeltaNode {
    struct Vec2 delta;
    struct DeltaNode* prev;
};
struct DeltaLinkedList {
    struct DeltaNode deltas[MAX_CACHED_DELTAS];
    uint8_t cache_index;
    uint8_t num_cached;
};

void ll_add_delta(struct DeltaLinkedList* ll, struct Vec2 delta) {
    struct DeltaNode* head = &ll->deltas[ll->cache_index];
    uint8_t new_index = ll->cache_index + 1;
    if (ll->num_cached == 0) {
        head = NULL;
        new_index = 0;
    } else if (ll->cache_index == MAX_CACHED_DELTAS) {
        new_index = 0;
    }

    struct DeltaNode* new_node = &ll->deltas[new_index];
    new_node->delta = delta;
    new_node->prev = head;
    ll->cache_index = new_index;
    ll->num_cached++;
    if (ll->num_cached > MAX_CACHED_DELTAS) {
        ll->num_cached = MAX_CACHED_DELTAS;
    }
}

struct Vec2 ll_sum(struct DeltaLinkedList* ll, struct DeltaNode* start) {
    struct Vec2 sum = {0.f, 0.f};
    struct DeltaNode* head = NULL;
    if (start != NULL) {
        head = start;
    } else {
        head = &ll->deltas[ll->cache_index];
    }
    do {
        sum = v2_add(&sum, &head->delta);
        head = head->prev;
    } while (head->prev != NULL);

    return sum;
}

bool ll_is_ccw_rotation(struct DeltaLinkedList* ll) {
    // struct Vec2 head_vec;
    // struct Vec2 tail_vec;

    // if (ll->num_cached < 2) {
    if (ll->num_cached < MAX_CACHED_DELTAS) {
        return false;
    }

    // struct DeltaNode* head = &ll->deltas[ll->cache_index];
    // struct Vec2 end_point = ll_sum(ll);
    // struct Vec2 end_point = head->delta;
    // walk back through the cached nodes until we get to the start
    // struct DeltaNode* tail = head->prev;
    // while (tail->prev != NULL) {
    //     end_point = v2_add(&end_point, &tail->delta);
    //     tail = tail->prev;
    // }

    return false;
}

void ll_clear(struct DeltaLinkedList* ll) {
    ll->cache_index = 0;
    ll->num_cached = 0;
    ll->deltas[0] = (struct DeltaNode){(struct Vec2){0.f, 0.f}, NULL};
}

struct DeltaLinkedList cached_deltas;

// Trackball State
bool  is_scroll_clicked    = false;
bool  is_drag_scroll       = false;
float scroll_accumulated_h = 0;
float scroll_accumulated_v = 0;
bool  is_dpi_held          = false;
float flip_accumulated     = 0;
bool  is_tab_flip          = false;
bool previous_rotation_ccw = true;
bool  is_rot_scroll        = false;

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

    // add cached pointer deltas (and latest)
    uprintf("Adding new pointer delta: (%f, %f)\n", (float)x, (float)y);
    ll_add_delta(&cached_deltas, (struct Vec2){(float)x, (float)y});
    // only do the expensive stuff if we have enough cached deltas to calculate rotation
    if (cached_deltas.num_cached >= MAX_CACHED_DELTAS) {
        uprintf("Enough deltas to calculate rotation\n");
        struct DeltaNode* head = &cached_deltas.deltas[cached_deltas.cache_index];
        struct Vec2 end_point = ll_sum(&cached_deltas, NULL);

        uprintf("Rotation end point: (%f, %f)\n", end_point.x, end_point.y);
        // We could get _an_ angle between the start and end points using atan2f(cross, dot),
        // but this would be with the origin at 0,0 and would not reflect the real radius of the arc
        // being drawn with the pointer rotation.
        // Instead we can use the cross product to find the direction of rotation, then
        // find the width and height of the arc being traced to find the radius
        //
        // I think the cross product really only checks that it's more than 180 degrees
        // so technically a very fast rotation could trigger the wrong direction?

        // const float cross = v2_cross(&head->delta, &end_point);
        // const bool is_ccw = cross >= 0.f;

        const float w = v2_length(&end_point);
        uprintf("W: %f\n", w);
        struct Vec2 n = v2_normalize(&end_point);
        uprintf("N: (%f, %f)\n", n.x, n.y);
        {
            // get the perpendicular vector
            float t = n.x;
            n.x = -n.y;
            n.y = t;
        }

        // use cross product of first and last position to get the rotation direction
        bool is_ccw = v2_cross(&head->delta, &end_point) >= 0.f;
        uprintf("Rotation counter-clockwise: %d\n", is_ccw);

        // To find the height of the arc, we need to traverse the cached deltas and
        // project every interior point of the arc onto the normal of (end - start)
        // This magnitude of the projection should be a decent approximation of arc height
        // (we can actually consider start as (0,0) since end_point is just deltas)
        const struct Vec2 mid_point = v2_scalar_mul(&end_point, 2.f);
        uprintf("Mid-point: (%f, %f)\n", mid_point.x, mid_point.y);
        uint8_t i = 0;
        float height = 0.f;
        struct Vec2 interior_point = end_point;
        while(head->prev != NULL) {
            if (i > 0 && i <= MAX_CACHED_DELTAS) {
                const struct Vec2 interior_adjusted = v2_sub(&end_point, &mid_point);
                // struct Vec2 interior_n = v2_normalize(&interior_adjusted);
                // float cos_theta = v2_dot(&interior_n, &n);
                float cos_theta = v2_dot(&interior_adjusted, &n);
                // height = fmax(height, cos_theta * v2_length(&interior_adjusted));
                uprintf("Height %d: %f\n", i, cos_theta);
                height = fmax(height, cos_theta);
            }
            interior_point = v2_sub(&interior_point, &head->delta);
            head = head->prev;
            ++i;
        }
        uprintf("Max height: %f\n", height);

        // r = (H^2 + (W/2)^2) / 2H
        //float radius = (powf(height, 2.f) + powf(w / 2.f, 2.f)) / (2.f * height);
        if (height > 0.f) {
            float radius = (height / 2.f) + (powf(w, 2.f) / (height * 8.f));
            uprintf("Radius: %f\n", radius);
            if (radius > 0.f) {
                result = 2 * acos((radius - height) / radius) * (is_ccw ? -1.f : 1.f);
                uprintf("Angle of rotation: %f\n", result);
            } else {
                uprintf("Something wrong: radius zero %f\n", radius);
            }
        } else {
            uprintf("Something wrong: height zero\n");
        }
    }

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
    } else if (is_rot_scroll) {
        uprintf("Rotation scroll sampling\n");
        scroll_accumulated_v += get_pointer_rotation(mouse_report.x, mouse_report.y);
        uprintf("Accumulated rotation: %f\n", scroll_accumulated_v);

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
            ll_clear(&cached_deltas);
            previous_rotation_ccw = true;
            previous_pvector = (struct Vec2){0.f, 0.f};
            previous_pnormal_left = (struct Vec2){0.f, 0.f};
            previous_pnormal_right = (struct Vec2){0.f, 0.f};
            is_tab_flip = true;
        } else {
            ll_clear(&cached_deltas);
            is_tab_flip = false;
        }
    }

    if (keycode == ROT_SCROLL) {
        if (record->event.pressed) {
            ll_clear(&cached_deltas);
            previous_pvector = (struct Vec2){0.f, 0.f};
            previous_pnormal_left = (struct Vec2){0.f, 0.f};
            previous_pnormal_right = (struct Vec2){0.f, 0.f};
            is_rot_scroll = true;
        } else {
            is_rot_scroll = false;
            ll_clear(&cached_deltas);
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
