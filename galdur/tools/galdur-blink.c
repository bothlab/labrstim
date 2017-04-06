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

static gint opt_delay  = 500;


static volatile gboolean blink_running = TRUE;

void
handle_sigint (int dummy)
{
    blink_running = FALSE;
}

void
run_status_led_blink ()
{
    uint8_t status_val = GLD_GPIO_LOW;

    if (opt_delay < 0) {
        g_error ("A negative delay makes no sense, we can not travel in time yet.");
        return;
    }

    while (blink_running) {
        status_val = ~status_val;
        gld_status_led_set (status_val);
        usleep (opt_delay * 1000);
    }
}

static GOptionEntry option_entries[] =
{
    { "delay", 'd', 0, G_OPTION_ARG_INT, &opt_delay,
        "Blink delay in msec", "Blink delay" },

    { NULL }
};

int main(int argc, char **argv)
{
    GError *error = NULL;
    GOptionContext *octx;

    /* parse our options */
    octx = g_option_context_new ("- Galdur Status LED Blink");
    g_option_context_add_main_entries (octx, option_entries, NULL);
    g_option_context_set_help_enabled (octx, TRUE);
    if (!g_option_context_parse (octx, &argc, &argv, &error)) {
        g_printerr ("Option parsing failed: %s\n", error->message);
        return 1;
    }

    if (!gld_board_initialize ())
        return 1;

    signal(SIGINT, handle_sigint);
    run_status_led_blink ();

    gld_status_led_set (GLD_GPIO_LOW);
    gld_board_shutdown ();
    return 0;
}
