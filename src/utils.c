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

#include "utils.h"

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <galdur.h>

#include "defaults.h"

/**
 * labrstim_make_realtime:
 *
 * Declare the program as a reat time program
 * (the user will need to be root to do this)
 */
gboolean
labrstim_make_realtime (const gchar *app_name)
{
    /* set scheduler policy */
    struct sched_param param;
    param.sched_priority = LS_PRIORITY;
    if (sched_setscheduler (0, SCHED_FIFO, &param) == -1) {
        g_printerr ("%s : sched_setscheduler failed\n", app_name);
        g_printerr ("Do you have permission to run real-time applications? You might need to be root or use sudo\n");
        return FALSE;
    }

    /* ensure the main thread does never run on the DAQ core */
    if (gld_set_thread_no_cpu_affinity (0) < 0) {
        g_printerr ("Unable to set CPU core affinity.");
        return FALSE;
    }

    /* lock memory */
    if (mlockall (MCL_CURRENT | MCL_FUTURE) == -1) {
        g_printerr ("%s: mlockall failed", app_name);
        return FALSE;
    }
    /* pre-fault our stack */
    unsigned char dummy[MAX_SAFE_STACK];
    memset (&dummy, 0, MAX_SAFE_STACK);

    return TRUE;
}
