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

#include "gld-gpio.h"

#include "bcm2835.h"

/**
 * gld_status_led_set:
 */
void
gld_status_led_set (uint8_t state)
{
    /* set status LED on pin 17 */
    bcm2835_gpio_write (RPI_GPIO_P1_11, state);
}

/**
 * gld_gpio_set_mode:
 */
void
gld_gpio_set_mode (GldGPIOPin pin, GldGPIOMode mode)
{
    if (mode == GLD_GPIO_MODE_OUTPUT)
        bcm2835_gpio_fsel (pin, BCM2835_GPIO_FSEL_OUTP);
    else if (mode == GLD_GPIO_MODE_INPUT)
        bcm2835_gpio_fsel (pin, BCM2835_GPIO_FSEL_INPT);
    else if (mode == GLD_GPIO_MODE_PULLUP)
        bcm2835_gpio_fsel (pin, BCM2835_GPIO_PUD_UP);
}

/**
 * gld_gpio_set_value:
 */
void
gld_gpio_set_value (GldGPIOPin pin, uint8_t state)
{
    bcm2835_gpio_write (pin, state);
}

/**
 * gld_gpio_read_level:
 */
uint8_t
gld_gpio_read_level (GldGPIOPin pin)
{
    return bcm2835_gpio_lev (pin);
}
