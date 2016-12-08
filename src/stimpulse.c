/*
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
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

#include "stimpulse.h"

#include <glib.h>

#include "gpio.h"

/**
 * stimpulse_init_gpio:
 */
void
stimpulse_gpio_init (void)
{
    gpio_export (GPIO_PIN_INTENSITY);
    gpio_export (GPIO_PIN_TRIGGER);

    gpio_set_direction (GPIO_PIN_INTENSITY, GPIO_OUT);
    gpio_set_direction (GPIO_PIN_TRIGGER, GPIO_OUT);
}

/**
 * stimpulse_set_intensity:
 */
void
stimpulse_set_intensity (double value)
{
    gpio_write (GPIO_PIN_INTENSITY, value);
}

/**
 * stimpulse_set_trigger_high:
 */
void
stimpulse_set_trigger_high (void)
{
    gpio_write (GPIO_PIN_TRIGGER, GPIO_HIGH);
}

/**
 * stimpulse_set_trigger_low:
 */
void
stimpulse_set_trigger_low (void)
{
    gpio_write (GPIO_PIN_TRIGGER, GPIO_LOW);
}
