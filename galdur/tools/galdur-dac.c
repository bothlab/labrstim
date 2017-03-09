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

static guint opt_delay  = 10;

static volatile gboolean g_running = TRUE;

void
handle_sigint (int dummy)
{
    g_running = FALSE;
}

void
run_dac ()
{
    const uint16_t max_value = 65535;
    const uint16_t min_value = 0;
    uint16_t value = 0;
    gboolean count_down = FALSE;

    while (g_running) {
        gld_dac_set_value (value);

        if (count_down)
            value--;
        else
            value++;
        if (value >= max_value)
            count_down = TRUE;
        if (value <= min_value)
            count_down = FALSE;
        usleep (opt_delay * 1000);
    }
}

static GOptionEntry option_entries[] =
{
    { "delay", 'd', 0, G_OPTION_ARG_INT, &opt_delay,
        "Delay between the steps in msec", "Wait delay" },

    { NULL }
};

int main(int argc, char **argv)
{
    GError *error = NULL;
    GOptionContext *octx;

    /* parse our options */
    octx = g_option_context_new ("- Galdur AUX SPI Test");
    g_option_context_add_main_entries (octx, option_entries, NULL);
    g_option_context_set_help_enabled (octx, TRUE);
    if (!g_option_context_parse (octx, &argc, &argv, &error)) {
        g_printerr ("Option parsing failed: %s\n", error->message);
        return 1;
    }

    if (!gld_board_initialize ())
        return 1;

    signal(SIGINT, handle_sigint);
    run_dac ();

    gld_dac_set_value (0);
    gld_board_shutdown ();
    return 0;
}
