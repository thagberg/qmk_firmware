/* Copyright 2023 Colin Lam (Ploopy Corporation)
 * Copyright 2020 Christopher Courtney, aka Drashna Jael're  (@drashna) <drashna@live.com>
 * Copyright 2019 Sunjun Kim
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
#include QMK_KEYBOARD_H

#define PLOOPY_DRAGSCROLL_MOMENTARY
#define PLOOPY_DRAGSCROLL_DIVISOR_H 12.0
#define PLOOPY_DRAGSCROLL_DIVISOR_V 12.0
#define PLOOPY_DRAGSCROLL_INVERT

// const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
//     [0] = LAYOUT( MS_BTN4, MS_BTN5, DRAG_SCROLL, MS_BTN2, MS_BTN1, MS_BTN3 )
// };

                // {"matrix": [0, 1], "label":"Top Left Left", "x":0, "y":0, "h":2},
                // {"matrix": [0, 2], "label":"Top Left", "x":1.25, "y":0, "h":1.25},
                // {"matrix": [0, 3], "label":"Top Right", "x":2.5, "y":0, "h":1.25},
                // {"matrix": [0, 4], "label":"Top Right Right", "x":3.75, "y":0, "h":2},
                // {"matrix": [0, 0], "label":"Bottom Left", "x":0, "y":2.25, "w":1.75, "h":2},
                // {"matrix": [0, 5], "label":"Bottom Right", "x":3, "y":2.25, "w":1.75, "h":2}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    // [0] = LAYOUT( MS_BTN4, MS_BTN5, DRAG_SCROLL, MS_BTN2, MS_BTN1, MS_BTN3 )
    [0] = LAYOUT( DRAG_SCROLL, DPI_CONFIG, MS_BTN5, MS_BTN2, DPI_HOLD, MS_BTN1 )
};
