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

#include "gld-utils.h"

/**
 * gld_set_timespec_from_ms:
 */
struct timespec
gld_set_timespec_from_ms (double milisec)
{
    // set the values in timespec structure
    struct timespec temp;
    time_t sec = (int) (milisec / 1000);
    milisec = milisec - (sec * 1000);
    temp.tv_sec = sec;
    temp.tv_nsec = milisec * 1000000L;

    return temp;
}

/**
 * gld_time_diff:
 */
struct timespec
gld_time_diff (struct timespec* start, struct timespec* end)
{
    // get the time difference between two times
    struct timespec temp;
    if ((end->tv_nsec-start->tv_nsec) < 0) {
        temp.tv_sec = end->tv_sec - start->tv_sec - 1;
        temp.tv_nsec = 1000000000 + end->tv_nsec - start->tv_nsec;
    } else {
        temp.tv_sec = end->tv_sec - start->tv_sec;
        temp.tv_nsec = end->tv_nsec - start->tv_nsec;
    }

    return temp;
}

/**
 * microsecond_from_timespec:
 */
int
gld_microsecond_from_timespec (struct timespec* duration)
{
    int ms;
    ms = duration->tv_nsec / 1000;

    //  ms=ms+duration.tv_sec*1000;
    return ms;
}
