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

#ifndef __LS_TASKS_H
#define __LS_TASKS_H

#include <glib.h>

#include "data-file-si.h"

gboolean
perform_train_stimulation (gboolean random,
                           double trial_duration_sec,
                           double pulse_duration_ms,
                           double minimum_interval_ms,
                           double maximum_interval_ms,
                           double train_frequency_hz);

gboolean
perform_theta_stimulation (gboolean random,
                           double trial_duration_sec,
                           double pulse_duration_ms,
                           data_file_si *offline_data_file);

gboolean
perform_swr_stimulation (double trial_duration_sec,
                         double pulse_duration_ms,
                         data_file_si *offline_data_file);


#endif /* __LS_TASKS_H */
