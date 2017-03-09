/*
 * Copyright (C) 2017 Matthias Klumpp <matthias@tenstral.net>
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

#include "gld-dac.h"

#include "bcm2835.h"


static void print_binary (unsigned int val, size_t size)
{
    unsigned int n = val;
    size_t pos = size - 1;

    while (pos != 0) {
        if (n & 1)
            g_print ("1");
        else
            g_print ("0");

        n >>= 1;
        pos--;
    }
    g_print ("\n");
}

/**
 * gld_dac_set_value:
 */
void
gld_dac_set_value (uint16_t value)
{
    uint8_t tx_ctl;

    tx_ctl = 0b00110001;

    bcm2835_aux_spi_writenb ((char*) &tx_ctl, sizeof(tx_ctl));
    bcm2835_aux_spi_writenb ((char*) &value, sizeof(value));
}
