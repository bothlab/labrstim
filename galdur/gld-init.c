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

#include "gld-init.h"

#include <stdio.h>
#include <stdlib.h>

#include "bcm2835.h"

/**
 * gld_cpu_is_valid:
 */
static gboolean
gld_cpu_is_valid (void)
{
    FILE *fp;
    gchar *line = NULL;
    size_t len = 0;
    ssize_t read;
    gboolean ret = FALSE;

    fp = fopen ("/proc/cpuinfo", "r");
    if (fp == NULL)
        return FALSE;

    while ((read = getline(&line, &len, fp)) != -1) {
        if (g_str_has_prefix (line, "Hardware")) {
            if (g_strrstr (line, "BCM2835") != NULL) {
                ret = TRUE;
                goto out;
            }

            g_debug ("CPU Hardware: %s", line);
        }
    }

out:
    fclose (fp);
    if (line)
        g_free (line);

    return ret;
}

/**
 * gld_board_initialize:
 *
 * Initialize all connections to the Galdur board.
 */
gboolean
gld_board_initialize (void)
{
    if (!gld_cpu_is_valid ()) {
        g_error ("You are not running on the right BCM-2835 or compatible CPU. This tool currently only works on a Raspberry Pi 3.");
        return FALSE;
    }

    if (!bcm2835_init ()) {
      g_error ("bcm2835_init failed. Is the software running on the right CPU and with appropriate permissions?");
      return FALSE;
    }

    /* set up BCM2835 */
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_128); /* the right clock divider for RasPi 3 B+ */
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0); /* chip-select 0 */
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);

    if (!bcm2835_spi_begin ()) {
      g_error ("bcm2835_spi_begin failed. Is the software running on the right CPU?");
      return FALSE;
    }

    if (!bcm2835_aux_spi_begin ()) {
        g_error ("bcm2835_aux_spi_begin failed.");
        return FALSE;
    }

    /* initialize default GPIO pins */

    /* status LED on pin 17 */
    bcm2835_gpio_fsel (RPI_GPIO_P1_11, BCM2835_GPIO_FSEL_OUTP);

    return TRUE;
}

/**
 * gld_board_shutdown:
 *
 * Close all connections to the Galdur board and finalize global state.
 */
void
gld_board_shutdown (void)
{
    /* reset everything */
    bcm2835_spi_end ();
    bcm2835_aux_spi_end ();
    bcm2835_close ();
}

/**
 * gld_board_set_spi_clock_divider:
 */
void
gld_board_set_spi_clock_divider (const uint16_t divider)
{
    bcm2835_spi_setClockDivider (divider);
}

/**
 * gld_board_set_aux_spi_clock_divider:
 */
void
gld_board_set_aux_spi_clock_divider (const uint16_t divider)
{
    bcm2835_aux_spi_setClockDivider (divider);
}
