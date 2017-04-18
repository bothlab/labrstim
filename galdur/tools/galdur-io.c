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

#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "galdur.h"

static gint opt_channel_count = 1;

static volatile gboolean g_running = TRUE;

/**
 * handle_sigint:
 */
void
handle_sigint (int dummy)
{
    g_running = FALSE;
}

/**
 * run_io_passthrough:
 */
int
run_io_passthrough ()
{
    GldAdc *adc;

    if ((opt_channel_count > 4) || (opt_channel_count < 1)) {
        g_printerr ("Please select a channel count between 1 and 4.\n");
        return 2;
    }

    /* be as fast as we (and the DAC chip) possibly can on the AUX SPI */
    gld_board_set_aux_spi_clock_divider (2);

    /* new ADC interface with default buffer size */
    adc = gld_adc_new (opt_channel_count, 0, -1);
    while (g_running) {
        int16_t invalue;
        guint i;

        /* fetch single set of data */
        gld_adc_acquire_single_dataset (adc);

        for (i = 0; i < opt_channel_count; i++) {
            if (gld_adc_get_sample (adc, i, &invalue)) {
                /* shift bipolar value up, so we get a positive int covering the full range */
                uint16_t outvalue = invalue + (INT16_MIN * -1);

                /* pass acquired value to the DAC */
                gld_dac_set_value (i, outvalue);
            } else {
                g_print ("Unable obtain piece of ADC buffer. Data incomplete.\n");
            }
        }
    }
    gld_adc_free (adc);

    return 0;
}

static GOptionEntry option_entries[] =
{
    { "channels", 'n', 0, G_OPTION_ARG_INT, &opt_channel_count,
        "Number of channels to read from / write to (1-4)", "Channel count" },

    { NULL }
};

/**
 * main:
 */
int main(int argc, char **argv)
{
    GError *error = NULL;
    int ret;
    GOptionContext *octx;

    /* parse our options */
    octx = g_option_context_new ("- Galdur IO Passthrough Test");
    g_option_context_add_main_entries (octx, option_entries, NULL);
    g_option_context_set_help_enabled (octx, TRUE);
    if (!g_option_context_parse (octx, &argc, &argv, &error)) {
        g_printerr ("Option parsing failed: %s\n", error->message);
        return 1;
    }

    if (!gld_board_initialize ())
        return 1;

    signal(SIGINT, handle_sigint);
    ret = run_io_passthrough ();

    if (ret == 0) {
        guint i;

        /* reset output */
        for (i = 0; i < opt_channel_count; i++)
            gld_dac_set_value (i, 0);
    }
    gld_board_shutdown ();

    return ret;
}
