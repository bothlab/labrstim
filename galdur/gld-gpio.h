/*
 * Copyright (C) 2016-2017 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
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

#ifndef __GLD_GPIO_H
#define __GLD_GPIO_H

#include <stdint.h>

#define GLD_GPIO_HIGH 1
#define GLD_GPIO_LOW  0

typedef enum {
    GLD_GPIO_PIN_05 = 5,
    GLD_GPIO_PIN_06 = 6,
    GLD_GPIO_PIN_12 = 12,
    GLD_GPIO_PIN_13 = 13,
    GLD_GPIO_PIN_18 = 18,
    GLD_GPIO_PIN_22 = 22,
    GLD_GPIO_PIN_23 = 23,
    GLD_GPIO_PIN_24 = 24,
    GLD_GPIO_PIN_25 = 25,
    GLD_GPIO_PIN_27 = 27
} GldGPIOPin;

typedef enum {
    GLD_GPIO_MODE_INPUT,
    GLD_GPIO_MODE_OUTPUT,
    GLD_GPIO_MODE_PULLUP
} GldGPIOMode;

void gld_status_led_set (uint8_t state);

void gld_gpio_set_mode (GldGPIOPin pin, GldGPIOMode mode);
void gld_gpio_set_value (GldGPIOPin pin, uint8_t state);
uint8_t gld_gpio_read_level (GldGPIOPin pin);

#endif /* __GLD_GPIO_H */
