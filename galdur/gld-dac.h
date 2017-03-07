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

#ifndef __GLD_DAQ_H
#define __GLD_DAQ_H

#include <glib.h>
#include <stdint.h>

typedef struct
{
    struct _DataBuffer **buffer;
    guint       channel_count;
    pthread_t   tid;
    volatile gboolean running;

    guint acq_frequency;
    size_t sample_max_count;
} Max1133Daq;

Max1133Daq      *max1133daq_new (guint channel_count,
                                 size_t buffer_capacity);
void            max1133daq_free (Max1133Daq *daq);

void            max1133daq_set_acq_frequency (Max1133Daq *daq,
                                              guint hz);

gboolean        max1133daq_acquire_data (Max1133Daq *daq,
                                         size_t sample_count);
gboolean        max1133daq_reset (Max1133Daq *daq);
gboolean        max1133daq_is_running (Max1133Daq *daq);

gboolean        max1133daq_get_data (Max1133Daq *daq,
                                     guint channel,
                                     int16_t *data);


#endif /* __GLD_DAQ_H */
