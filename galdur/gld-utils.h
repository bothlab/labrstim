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
int gld_microsecond_from_timespec (struct timespec* duration);

#define debugln(fmt, ...) \
            do { if (DEBUG_PRINT) g_debug (fmt, __VA_ARGS__); } while (0)

#endif /* __GLD_UTILS_H */
