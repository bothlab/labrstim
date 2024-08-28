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

#ifndef __GLD_UTILS_H
#define __GLD_UTILS_H

#include "config.h"
#include <time.h>
#include <glib.h>

struct timespec gld_set_timespec_from_ms (double milisec);
struct timespec gld_time_diff (struct timespec* start,
                      struct timespec* end);

int64_t     gld_microsecond_from_timespec (struct timespec* duration);
int64_t     gld_milliseconds_from_timespec (struct timespec* duration);

int         gld_set_thread_cpu_affinity (int cpu_id);
int         gld_set_thread_no_cpu_affinity (int cpu_id);

#ifdef DEBUG
 #define gld_debug(fmt, args...) g_print("DEBUG: %s:%d: " fmt, \
                                    __FILE__, __LINE__, ##args)
#else
 #define gld_debug(fmt, args...) /* don't do anything in release builds */
#endif

#endif /* __GLD_UTILS_H */
