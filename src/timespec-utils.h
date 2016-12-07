/*
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C) 2010 Kevin Allen
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

#ifndef __LS_TIMESPEC_UTILS_H
#define __LS_TIMESPEC_UTILS_H

#include <time.h>

/**
 * TimeKeeper:
 *
 * structure to hold the variables to do the time keeping
 */
typedef struct
{
    // timespec variables store time with high accuracy
    // time_ = time point
    // elapsed_ = duration

    double trial_duration_sec;
    double pulse_duration_ms;

    double inter_pulse_duration_ms; // for the train stimulation
    struct timespec inter_pulse_duration; // for train stimulation
    double end_to_start_pulse_ms; // for the train stimulation
    struct timespec swr_delay;
    double swr_delay_ms;
    struct timespec end_to_start_pulse_duration;
    struct timespec time_now;
    struct timespec time_beginning_trial;
    struct timespec elapsed_beginning_trial;
    struct timespec time_data_arrive_from_acquisition;
    struct timespec elapsed_from_acquisition;
    struct timespec time_last_stimulation;
    struct timespec time_end_last_stimulation;
    struct timespec elapsed_last_stimulation;
    struct timespec time_last_acquired_data;
    struct timespec elapsed_last_acquired_data;
    struct timespec time_processing;
    struct timespec elapsed_processing;
    struct timespec duration_pulse;
    struct timespec duration_refractory_period;
    struct timespec duration_sleep_when_no_new_data;
    struct timespec duration_sleep_to_right_phase;
    struct timespec actual_pulse_duration; // the duration of the last pulse, including nanosleep jitter
    struct timespec time_previous_new_data; // next 3 used in theta stimultion, to check the time to get new data
    struct timespec time_current_new_data;
    struct timespec duration_previous_current_new_data;
    struct timespec duration_sleep_between_swr_processing;
    struct timespec interval_duration_between_swr_processing;

    struct timespec req;
    // double nano_comp_ms=NANOSLEEP_OVERSHOOT;
} TimeKeeper;

struct timespec set_timespec_from_ms(double milisec);
struct timespec diff(struct timespec* start, struct timespec* end);
int microsecond_from_timespec(struct timespec* duration);

#endif /* __LS_TIMESPEC_UTILS_H */
