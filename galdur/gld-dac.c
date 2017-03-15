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


enum {
    DAC0 = 0x01,
    DAC1 = 0x02,
    DAC2 = 0x04,
    DAC3 = 0x08
} DACNumber;


/**
 * gld_dac_set_value:
 */
void
gld_dac_set_value (uint16_t value)
{
    struct __attribute__ ((packed)) {
        uint8_t control;
        int16_t data;
    } tx_cmd;


    tx_cmd.control = 0b00110000 | DAC0;
    tx_cmd.data    = (value << 8) | (value >> 8); // reverse byteorder

    bcm2835_aux_spi_writenb ((char*) &tx_cmd, sizeof(tx_cmd));
}
